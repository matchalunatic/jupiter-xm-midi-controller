"""Careful, this is evil code.

It takes the YAML spec extracted from the MIDI spec sheet and turns it into a full expression of the memory layout.

It will generate a huge file reasonably fast. You may peruse it.

In the future, I will make it more easily searchable.
"""

from __future__ import annotations
from typing import Any, Dict, Generator, List, Optional, Tuple
import logging
import yaml
from enum import IntEnum
from dataclasses import dataclass
import textwrap
import math
import re

logger = logging.getLogger(__name__)

ADDRESSABLE_MEMORY_SIZE = 0x80_00_00_00


def normalize_define_name(name: str):
    normalized = re.sub(r'_+', '_', re.sub(r'\W', '_', name)).strip('_')
    normalized = '_' + normalized if not normalized or not normalized[0].isalpha() else normalized
    return normalized.upper()

def drop_repeating_consecutive_words(normalized_name: str):
    spl = normalized_name.split('_')
    previous_component = ''
    o = []
    for component in spl:
        if component == previous_component:
            continue
        o.append(component)
        previous_component = component
    return '_'.join(o)

assert normalize_define_name('') == '_'
assert normalize_define_name('__foo') == 'FOO'

class ZoneKind(IntEnum):
    KIND_NON_STRIDING = 0
    KIND_STRIDING = 1


class Offset(int):
    def __new__(cls, value, length=None):
        n = int.__new__(cls, value)
        if length is None:
            try:
                length = math.ceil(math.log(value, 16))
                if length % 2 == 1:
                    length += 1
            except ValueError:  # 0 values
                length = 2
        n.length = length
        return n

    def __repr__(self):
        return hex(self)


class OffsetAddr(Offset):
    def __repr__(self):
        mask = r"{0:08x}"
        if self.length:
            mask = r"{0:0" + str(self.length) + r"x}"
        b = mask.format(self)
        return " ".join(b[r : r + 2] for r in range(0, len(b), 2))


EXCLUDE_MEMORY_RANGES = (
    (OffsetAddr(0x40000000), OffsetAddr(0x7FFFFFFF)),
    (OffsetAddr(0x40000000), OffsetAddr(0x7FFFFFFF)),
)


class OffsetStride(Offset):
    pass


class StructureTypeUniqueSymbol(str):
    _instances: Dict[str, StructureType] = {}

    def __new__(cls, value, for_structure=None):
        if value in cls._instances:
            logger.debug("StructureTypeUniqueSymbol[%s]: singleton", value)
            return cls._instances[value]
        s = str.__new__(cls, value)
        s.for_structure = for_structure
        cls._instances[value] = s
        logger.debug(
            "StructureTypeUniqueSymbol: Creating new instance %s (id: %s)", s, id(s)
        )
        return s

    @property
    def structure(self) -> StructureType:
        return self.for_structure

    @structure.setter
    def structure(self, val: StructureType):
        assert self.for_structure is None
        self.for_structure = val

    @classmethod
    def obtain_existing_structure_type(cls, name: str) -> StructureType:
        return cls._instances[name].structure


class TopLevelMemoryZoneUniqueSymbol(str):
    _instances: Dict[str, TopLevelMemoryZoneContainer] = {}

    def __new__(cls, value, for_zone=None):
        if value in cls._instances:
            logger.debug("TopLevelMemoryZoneUniqueSymbol[%s]: singleton", value)
            return cls._instances[value]
        s = str.__new__(cls, value)
        s.for_zone = for_zone
        cls._instances[value] = s
        logger.debug("TopLevelMemoryZoneUniqueSymbol: Creating new instance %s", s)
        return s

    @property
    def zone(self) -> TopLevelMemoryZoneContainer:
        return self.for_zone

    @zone.setter
    def zone(self, val: TopLevelMemoryZoneContainer):
        assert self.for_zone is None
        self.for_zone = val

    @classmethod
    def obtain_existing_zone_declaration(cls, name: str) -> TopLevelMemoryZoneContainer:
        return cls._instances[name].zonestructure


class TopLevelMetaStructureUniqueSymbol(str):
    _instances: Dict[str, TopLevelMetaStructureType] = {}

    def __new__(cls, value, for_metastructure=None):
        if value in cls._instances:
            logger.debug(
                "TopLevelMetaStructureUniqueSymbol[%s]: singleton = %s",
                value,
                cls._instances[value],
            )
            return cls._instances[value]
        s = str.__new__(cls, value)
        s.for_metastructure = for_metastructure
        cls._instances[value] = s
        logger.debug(
            "TopLevelMetaStructureUniqueSymbol: Creating new instance %s (id: %s)",
            s,
            id(s),
        )
        return s

    @property
    def metastructure(self) -> TopLevelMetaStructureType:
        return self.for_metastructure

    @metastructure.setter
    def metastructure(self, val: TopLevelMetaStructureType):
        assert self.for_metastructure is None
        self.for_metastructure = val

    @classmethod
    def obtain_existing_meta_structure_type(
        cls, name: str
    ) -> TopLevelMetaStructureType:
        return cls._instances[name].metastructure


@dataclass
class MemoryAddressView:
    offset: OffsetAddr
    index: int
    define_name: str
    length: int
    summary: str
    structure_type: StructureType
    structure_atom: StructureAtom
    value: Optional[Any] = None
    initiator: Optional[TopLevelMetaStructureInstance] = None

    @property
    def size(self):
        return max(1, self.structure_atom.last_offset_start - self.structure_atom.first_offset_start + 1)

    @property
    def normalized_define_name(self) -> str:
        if self.initiator is None:
            raise RuntimeError("Cannot produce normalized define name without initiator")
        
        is_one_instance_of_many = self.index > 0
        if is_one_instance_of_many:
            return drop_repeating_consecutive_words(normalize_define_name(f"{self.initiator.define_basename}_{self.structure_type.name}_{self.index}_{self.structure_atom.name}"))
        else:
            return drop_repeating_consecutive_words(normalize_define_name(f"{self.initiator.define_basename}_{self.structure_type.name}_{self.structure_atom.name}"))

@dataclass
class MemoryLayout:
    contents: List[TopLevelMemoryZoneContainer]


@dataclass
class TopLevelMemoryZone:
    """This represents an actual memory zone with a physical offset in memory.
    The contents of the zone may then be accessed.
    """

    container: TopLevelMemoryZoneContainer
    index: int

    @property
    def offset(self) -> OffsetAddr:
        parent_offset = 0
        if self.container.kind == ZoneKind.KIND_NON_STRIDING:
            ct: NonStridingTopLevelMemoryZoneContainer = self.container
            parent_offset = ct.offset
        else:
            ct: StridingTopLevelMemoryZoneContainer = self.container
            parent_offset = ct.first_offset + ct.stride * self.index
        return OffsetAddr(parent_offset)

    @property
    def contents(self) -> TopLevelMetaStructureInstance:
        metastructure_type = self.container.top_level_type.metastructure
        try:
            assert metastructure_type is not None
        except AssertionError:
            logger.error(
                "metastructure %s (id: %s) for container %s index %s: no good metastructure def",
                self.container.top_level_type,
                id(self.container.top_level_type),
                self.container,
                self.index,
            )
            raise
        return TopLevelMetaStructureInstance(metastructure_type, self)

    def __str__(self):
        return f"""TopLevelMemoryZone({self.container.name}): {self.index + 1}/{self.container.item_count}"""


@dataclass
class TopLevelMemoryZoneContainer:
    """These are abstractions to define actual types afterwards"""

    name: TopLevelMemoryZoneUniqueSymbol
    top_level_type: TopLevelMetaStructureUniqueSymbol

    def __post_init__(self):
        logger.debug(f"Setting top-level zone reference for symbol {self.name}")
        self.name.zone = self

    @property
    def kind(self) -> ZoneKind:
        raise NotImplementedError

    @property
    def item_count(self) -> int:
        raise NotImplementedError

    @property
    def contents(self) -> List[TopLevelMemoryZone]:
        raise NotImplementedError

    @property
    def normalized_name(self):
        return self.name.replace()

@dataclass
class StridingTopLevelMemoryZoneContainer(TopLevelMemoryZoneContainer):
    first_offset: OffsetAddr
    last_offset: OffsetAddr
    stride: OffsetStride

    @property
    def item_count(self):
        assert self.first_offset < self.last_offset
        return (self.stride + self.last_offset - self.first_offset) // self.stride

    @property
    def kind(self):
        return ZoneKind.KIND_STRIDING

    def __str__(self):
        return f"StridingTopLevelMemoryZoneContainer:{self.name} [{self.first_offset}..{self.last_offset}/{self.stride}] ({self.item_count} items)"

    @property
    def contents(self) -> List[TopLevelMemoryZone]:
        # obtain the real memory zones
        return list(self.contents_gen())

    def contents_gen(self):
        for index in range(self.item_count):
            yield TopLevelMemoryZone(self, index)


@dataclass
class NonStridingTopLevelMemoryZoneContainer(TopLevelMemoryZoneContainer):
    offset: OffsetAddr

    def __str__(self):
        return f"NonStridingTopLevelMemoryZoneContainer:{self.name}[{self.offset}]"

    @property
    def kind(self) -> ZoneKind:
        return ZoneKind.KIND_NON_STRIDING

    @property
    def item_count(self) -> int:
        return 1

    @property
    def contents(self) -> List[TopLevelMemoryZone]:
        return [TopLevelMemoryZone(self, 0)]


@dataclass
class TopLevelMetaStructureType:
    name: TopLevelMetaStructureUniqueSymbol
    structure_refs: List[TopLevelMetaStructureStructureRef]

    def __post_init__(self):
        logger.debug(
            f"Setting metastructure reference for {self.name} [id: {id(self.name)}]"
        )
        self.name.metastructure = self

    def __str__(self):
        srefs = "\n            ".join(str(s) for s in self.structure_refs)
        return textwrap.dedent(f"""\
        TopLevelMetaStructure({self.name}):
            {srefs}
        """)


@dataclass
class TopLevelMetaStructureInstance:
    meta_structure: TopLevelMetaStructureType
    memory_zone: TopLevelMemoryZone

    @property
    def define_basename(self) -> str:
        """Generate a C #define basename for all atoms in this metastructure"""
        if self.memory_zone.container.item_count > 1:
            # multiple items
            return f"{self.memory_zone.container.name}_{self.index}"
        else:
            return f"{self.memory_zone.container.name}"

    @property
    def offset(self) -> OffsetAddr:
        return self.memory_zone.offset

    @property
    def index(self) -> int:
        return self.memory_zone.index

    @property
    def total_offset(self):
        assert self.memory_zone.offset < ADDRESSABLE_MEMORY_SIZE
        return OffsetAddr(self.offset)

    @property
    def memory_view(self) -> Generator[MemoryAddressView]:
        """generate a list of offsets with each parameter and exact address"""
        base_offset = self.total_offset
        if any(a <= base_offset <= b for (a, b) in EXCLUDE_MEMORY_RANGES):
            return
        for el in self.meta_structure.structure_refs:
            # delegate to the structure ref the role
            for i in el.relative_memory_view(base_offset, self):
                yield i

    @property
    def memory_contents_report(
        self,
    ) -> Dict[OffsetAddr, Tuple[str, int, int, str, StructureType]]:
        """Offset: Memory zone name, Memory zone index, Structure index, Structure name, Structure type, Structure iteration"""
        out = {}
        memzone_name = self.memory_zone.container.name
        memzone_index = self.index

        for structure_index, el in enumerate(self.meta_structure.structure_refs):
            nsms: NonStridingTopLevelMetaStructureStructureRef = None
            sms: StridingTopLevelMetaStructureStructureRef = None
            if el.kind == ZoneKind.KIND_STRIDING:
                sms = el
            else:
                nsms = el

            if sms is not None:
                for num_iter in range(sms.item_count):
                    instance_offset = sms.stride * num_iter
                    total_off = OffsetAddr(
                        self.total_offset + instance_offset + sms.first_offset
                    )
                    assert total_off < ADDRESSABLE_MEMORY_SIZE
                    out[total_off] = (
                        memzone_name,
                        memzone_index,
                        structure_index + 1,
                        sms.structure_type.name,
                        num_iter + 1,
                    )
            elif nsms is not None:
                total_off = OffsetAddr(self.total_offset + nsms.offset)
                assert total_off < ADDRESSABLE_MEMORY_SIZE
                out[total_off] = (
                    memzone_name,
                    memzone_index,
                    structure_index + 1,
                    nsms.structure_type.name,
                    1,
                )
        return out


@dataclass
class TopLevelMetaStructureStructureRef:
    name: str
    structure_type: StructureType

    @property
    def kind(self):
        raise NotImplementedError

    def relative_memory_view(
        self, external_offset: OffsetAddr, initiator: TopLevelMetaStructureInstance
    ) -> Generator[MemoryAddressView]:
        raise NotImplementedError


@dataclass
class NonStridingTopLevelMetaStructureStructureRef(TopLevelMetaStructureStructureRef):
    offset: OffsetAddr

    def __str__(self):
        return (
            f"NonStridingTopLevelMetaStructureStructureRefs:{self.name}[{self.offset}]"
        )

    @property
    def kind(self):
        return ZoneKind.KIND_NON_STRIDING

    def relative_memory_view(
        self, external_offset: OffsetAddr, initiator: TopLevelMetaStructureInstance
    ) -> Generator[MemoryAddressView]:
        base_offset = self.offset
        assert base_offset < ADDRESSABLE_MEMORY_SIZE
        for atom in self.structure_type.atoms:
            summary_str = f"{initiator.meta_structure.name}[{initiator.index}].{self.name}[1(unique)].{atom.name}"
            define_name = f"{initiator.define_basename}_{self.name}_{atom.name}"
            total_offset = OffsetAddr(
                base_offset + external_offset + atom.first_offset_start
            )
            assert total_offset < ADDRESSABLE_MEMORY_SIZE

            yield MemoryAddressView(
                offset=total_offset,
                index=0,
                define_name=define_name,
                summary=summary_str,
                length=atom.length,
                structure_type=self.structure_type,
                structure_atom=atom,
                value="not implemented",
                initiator=initiator,
            )


@dataclass
class StridingTopLevelMetaStructureStructureRef(TopLevelMetaStructureStructureRef):
    first_offset: OffsetAddr
    last_offset: OffsetAddr
    stride: OffsetStride

    @property
    def item_count(self):
        if self.stride is None or self.first_offset == self.last_offset:
            return 1
        return (self.stride + self.last_offset - self.first_offset) // self.stride

    @property
    def kind(self):
        return ZoneKind.KIND_STRIDING

    def relative_memory_view(
        self, external_offset: OffsetAddr, initiator: TopLevelMetaStructureInstance
    ) -> Generator[MemoryAddressView]:
        for index, instance_offset in enumerate(
            range(self.first_offset, self.last_offset + 1, self.stride)
        ):
            for atom in self.structure_type.atoms:
                summary_str = f"{initiator.meta_structure.name}[{initiator.index}].{self.name}[{index + 1}].{atom.name}"

                define_name = (
                    f"{initiator.define_basename}_{self.name}_{index + 1}_{atom.name}"
                )
                yield MemoryAddressView(
                    OffsetAddr(
                        external_offset + instance_offset + atom.first_offset_start
                    ),
                    index=index,
                    define_name=define_name,
                    length=atom.length,
                    summary=summary_str,
                    structure_type=self.structure_type,
                    structure_atom=atom,
                    value="not implemented",
                    initiator=initiator
                )

    def __str__(self):
        return f"StridingTopLevelMetaStructureStructureRefs:{self.name} [{self.first_offset}..{self.last_offset}/{self.stride}] ({self.item_count} items)"


@dataclass
class MemoryZoneInstance:
    name: str
    type: MemoryZoneType


# these have unique
@dataclass
class MemoryZoneType:
    name: TopLevelMemoryZoneUniqueSymbol
    components: List[MemoryZoneInstance]


@dataclass
class NonStridingEntry(MemoryZoneInstance):
    offset: Offset


@dataclass
class StridingEntry(MemoryZoneInstance):
    first_offset: Offset
    last_offset: Offset
    stride: Offset


# these contain the actual business data
@dataclass
class StructureType:
    name: StructureTypeUniqueSymbol
    atoms: List[StructureAtom]

    def __post_init__(self):
        logger.debug(f"Setting structure reference for {self.name}")
        self.name.for_structure = self

    def __repr__(self):
        atom_repr = """
            """.join(str(a) for a in self.atoms)
        return textwrap.dedent(f"""\
        Structure({self.name}):
            {atom_repr}
        """)


@dataclass
class StructureAtom:
    name: str
    structure: StructureTypeUniqueSymbol
    first_offset_start: Offset
    last_offset_start: Offset
    bitmask: Offset
    discrete_range_low: int
    discrete_range_high: int
    human_value_base: Optional[int] = None
    human_value_list: Optional[List[str]] = None
    human_value_units: Optional[str] = None
    """possible props: 
        {
            'name': 2330
            'first_offset_start': 2330,
            'last_offset_start': 2330, 
            'bitmask': 2330,
            'discrete_range_low': 2330,
            'discrete_range_high': 2330,
            'human_value_base': 595, # optional 3 values
            'human_value_list': 1409,
            'human_value_units': 110,
            }
    """

    @property
    def length(self):
        return self.last_offset_start + 1 - self.first_offset_start

    def __str__(self):
        values_possible = f"{self.discrete_range_low}...{self.discrete_range_high}"
        if self.human_value_list:
            values_possible = ", ".join(self.human_value_list)
            if len(values_possible) > 40:
                last_values_possible = ", ".join(self.human_value_list[-2:])
                values_possible = f"{values_possible[0:40]}...{last_values_possible}"
        return f"Atom[{self.name}] ({self.first_offset_start}+{self.length})/{self.bitmask}: {values_possible}"


def walk_root(type_entries: dict) -> List[TopLevelMemoryZoneContainer]:
    entries = []
    for entry in type_entries["memory_layout"]:
        (name, fos, los, tlt, stride) = (
            entry.get(a)
            for a in (
                "name",
                "first_offset_start",
                "last_offset_start",
                "top_level_type",
                "stride",
            )
        )
        if stride is None:
            assert fos == los
            entries.append(
                NonStridingTopLevelMemoryZoneContainer(
                    TopLevelMemoryZoneUniqueSymbol(name),
                    TopLevelMetaStructureUniqueSymbol(tlt),
                    OffsetAddr(fos),
                )
            )
        else:
            entries.append(
                StridingTopLevelMemoryZoneContainer(
                    TopLevelMemoryZoneUniqueSymbol(name),
                    TopLevelMetaStructureUniqueSymbol(tlt),
                    OffsetAddr(fos),
                    OffsetAddr(los),
                    OffsetStride(stride),
                )
            )
    return entries


def parse_structlayouts(
    structlayout_entries: dict, size_entries: dict
) -> List[StructureType]:
    entries = []
    section_sizes = size_entries["size_entries"]
    raw_types = structlayout_entries["raw_unstructured_types"]
    for en, contents in structlayout_entries["data_structure_entries"].items():
        structure_name = StructureTypeUniqueSymbol(en)
        atoms = []
        if structure_name in raw_types:
            size = section_sizes[structure_name]
            logger.info(
                "Raw type %s of size %s, will not show details because it is extremely verbose.",
                structure_name,
                size,
            )
            atoms.append(
                StructureAtom(
                    "Fake Structure",
                    structure_name,
                    OffsetAddr(0),
                    OffsetAddr(size),
                    #Offset((1 << size) - 1), # proper but will be extremely long
                    Offset(0),
                    0,
                    0,
                )
            )
            contents = []
        for atom in contents:
            (name, fos, los, bitmask, drl, drh, hvb, hvl, hvu) = (
                atom.get(a)
                for a in (
                    "name",
                    "first_offset_start",
                    "last_offset_start",
                    "bitmask",
                    "discrete_range_low",
                    "discrete_range_high",
                    "human_value_base",
                    "human_value_list",
                    "human_value_units",
                )
            )
            atomObject = StructureAtom(
                name,
                structure_name,
                OffsetAddr(fos),
                OffsetAddr(los),
                Offset(bitmask),
                drl,
                drh,
                hvb,
                hvl,
                hvu,
            )
            atoms.append(atomObject)
        structure = StructureType(structure_name, atoms)
        entries.append(structure)
    return entries


def parse_top_level_meta_structures(memlayout_entries):
    top_level_meta_structures = memlayout_entries["top_level_meta_structures"]
    entries = []
    for tlt_name, elements in top_level_meta_structures.items():
        ms_structure_refs = []
        for sref in elements:
            (name, fos, los, struct_type, stride) = (
                sref.get(e)
                for e in (
                    "name",
                    "first_offset_start",
                    "last_offset_start",
                    "structure_type",
                    "stride",
                )
            )
            struct_type = StructureTypeUniqueSymbol(struct_type)
            try:
                assert (
                    struct_type.structure is not None
                )  # sanity check: do we know of this type
            except AssertionError:
                logger.critical(
                    "No type known for %s. Known types: %s",
                    struct_type,
                    StructureTypeUniqueSymbol._instances.keys(),
                )
                raise
            if stride is None:
                new_el = NonStridingTopLevelMetaStructureStructureRef(
                    name, struct_type.structure, OffsetAddr(fos)
                )
                ms_structure_refs.append(new_el)
            else:
                new_el = StridingTopLevelMetaStructureStructureRef(
                    name,
                    struct_type.structure,
                    OffsetAddr(fos),
                    OffsetAddr(los),
                    OffsetStride(stride),
                )
                ms_structure_refs.append(new_el)
        metastructure_name = TopLevelMetaStructureUniqueSymbol(tlt_name)
        entries.append(TopLevelMetaStructureType(metastructure_name, ms_structure_refs))
    return entries


def instantiate_memory(memory_layout: MemoryLayout):
    items_from_excluded_ranges = 0
    with open("define_list.h", "w", encoding="utf-8", newline="\n") as fh, open("jupiterx_addresses.py", "w", encoding="utf-8", newline="\n") as fh_p, open("jupiterx_items.py", "w", encoding="utf-8", newline="\n") as fh_i:
        fh_p.write("class JupiterXAddresses:\n")
        fh_i.write(textwrap.dedent('''\
                """Jupiter-X / XM memory addresses in a semi-convenient format for python"""
                from __future__ import annotations
                from collections import namedtuple
                JupXOffsetDetail = namedtuple('JupXOffsetDetail', ('offset', 'size', 'bitmask', 'range'))
                class JupiterXOffsetDetails:
                '''))
        for i in memory_layout.contents:
            for zone in i.contents:
                for report_item in zone.contents.memory_view:
                    if any(
                        a <= report_item.offset <= b for (a, b) in EXCLUDE_MEMORY_RANGES
                    ):
                        items_from_excluded_ranges += 1
                        if items_from_excluded_ranges % 100_000 == 0:
                            logger.info(
                                "Excluded items: %s", items_from_excluded_ranges
                            )
                        continue
                    formatted_offset = "0x{0:08x}".format(report_item.offset)
                    fh.write(
                        f"#define {report_item.normalized_define_name} {formatted_offset} // {report_item.summary}\n"
                        f"#define {report_item.normalized_define_name}_BITMASK {report_item.structure_atom.bitmask}\n"
                        f"#define {report_item.normalized_define_name}_SIZE {report_item.size}\n"
                        f"#define {report_item.normalized_define_name}_MIN {report_item.structure_atom.discrete_range_low}\n"
                        f"#define {report_item.normalized_define_name}_MAX {report_item.structure_atom.discrete_range_high}\n"
                    )
                    fh_p.write(f"    {report_item.normalized_define_name}_OFFSET: int = {formatted_offset}\n")
                    fh_p.write(f"    {report_item.normalized_define_name}_BITMASK: int = {report_item.structure_atom.bitmask}\n")
                    fh_p.write(f"    {report_item.normalized_define_name}_SIZE: int = {report_item.size}\n")
                    fh_p.write(f"    {report_item.normalized_define_name}_MIN: int = {report_item.structure_atom.discrete_range_low}\n")
                    fh_p.write(f"    {report_item.normalized_define_name}_MAX: int = {report_item.structure_atom.discrete_range_high}\n")

                    fh_i.write(f"    {report_item.normalized_define_name}: JupXOffsetDetail = JupXOffsetDetail(offset={formatted_offset}, size={report_item.size}, bitmask={report_item.structure_atom.bitmask}, range=({report_item.structure_atom.discrete_range_low}, {report_item.structure_atom.discrete_range_high}))\n")
    print(f"Excluded items: {items_from_excluded_ranges}")


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    memlayout_entries, structlayout_entries, size_entries = None, None, None
    with open("jupiter_memorylayout.yaml", "r", encoding="utf-8") as fh:
        memlayout_entries = yaml.load(fh, Loader=yaml.CLoader)
    with open("jupiter_sizes.yaml", "r", encoding="utf-8") as fh:
        size_entries = yaml.load(fh, Loader=yaml.CLoader)
    with open("jupiter_structlayout.yaml", "r", encoding="utf-8") as fh:
        structlayout_entries = yaml.load(fh, Loader=yaml.CLoader)

    raw_types = structlayout_entries["raw_unstructured_types"]
    root_entries = walk_root(memlayout_entries)
    struct_layouts = parse_structlayouts(structlayout_entries, size_entries)
    top_level_ms = parse_top_level_meta_structures(memlayout_entries)

    memory_layout = MemoryLayout(root_entries)
    for el in root_entries:
        print(f"Top-level zone: {el.name}: {el}")
    for el in struct_layouts:
        print(f"Struct: {el.name}")
    for el in top_level_ms:
        print(f"Top-level struct: {el.name}")
    instantiate_memory(memory_layout)
    # for el in root_entries:
