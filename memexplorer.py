#!/usr/bin/env python
"""Memory explorer module for Roland MIDI specs exported as YAML"""
from typing import Union, Optional
import logging
import anytree
import yaml
from jupiterlib import build_roland_rq1_message, get_sysex_data
from midi_utils import build_rtmidi_inout
from utils import lhex, nlhex, roland_address_arithmetic, roland_address_strider
from padme import proxy
from time import sleep
from adafruit_midi.system_exclusive import SystemExclusive

logging.basicConfig(level=logging.INFO)


logger = logging.getLogger(__name__)
logger.level = logging.INFO

            

class StrideProxyUtils:
    @classmethod
    def build_proxy(cls, obj, stride_wanted):
        if isinstance(obj, MemoryCategoryNode):
            p = StrideProxy(obj)
            proxy.state(p).stride_num = stride_wanted
            return p
        elif isinstance(obj, MemoryValueNode):
            p = StrideProxyForValues(obj)
            proxy.state(p).stride_num = stride_wanted
            proxy.state(p).parent_proxy = cls.build_proxy(p.parent, stride_wanted)
            return p
        else:
            logger.critical("build_proxy called on invalid item")
            raise TypeError("build_proxy with incompatible object")


class StrideProxyForValues(proxy):
    @proxy.direct
    def get_absolute_offset(self):
        org = proxy.original(self)
        prox_parent = proxy.state(self).parent_proxy
        return prox_parent.get_absolute_offset() + org.get_relative_offset()


    @proxy.direct
    def get_rq1_bytes(self):
        logger.debug("Requesting absolute offset from proxy")
        addr_bytes = self.get_absolute_offset().to_bytes(4, "big")
        data_bytes = self.get_data_length().to_bytes(4, "big")
        return addr_bytes, data_bytes


class StrideProxy(proxy):
    @proxy.direct
    def get_absolute_offset(self, stride_num=None):
        stride_wanted = proxy.state(self).stride_num
        if stride_num is not None:
            logger.warning(
                "Using stride_num %s in an already strided object. Will use stride %s",
                stride_num,
                stride_wanted,
            )
        org = proxy.original(self)
        logger.debug("Get absolute offset with stride %s", stride_wanted)
        assert org.is_in_a_stride()
        return org.get_absolute_offset(stride_wanted)

    @proxy.direct
    def get_relative_offset(self, stride_num=None):
        stride_wanted = proxy.state(self).stride_num
        if stride_num is not None:
            logger.warning(
                "Using stride_num %s in an already strided object. Will use stride %s",
                stride_num,
                stride_wanted,
            )
        org = proxy.original(self)
        assert org.is_in_a_stride()
        return org.get_relative_offset(stride_wanted)

    @proxy.direct
    def __repr__(self):
        stride_wanted = proxy.state(self).stride_num
        org = proxy.original(self)
        total_len = len(org)
        return f"{repr(org)} [{stride_wanted + 1}/{total_len}]"

    @proxy.direct
    def __str__(self):
        return repr(self)

    @proxy.direct
    def get(self, key):
        stride_wanted = proxy.state(self).stride_num
        org = proxy.original(self)
        r = org.get(key)
        return [StrideProxyUtils.build_proxy(a, stride_wanted) for a in r]

    @proxy.direct
    def memreport(self, with_value=False):
        if with_value:
            logger.warning("StrideProxy: with_value not implemented yet")
        stride_wanted = proxy.state(self).stride_num
        stride_wanted = proxy.state(self).stride_num
        org = proxy.original(self)
        nname = f'{self.name}[{stride_wanted + 1}/{len(org)}]'.ljust(40)
        ntype = self.type.ljust(10)
        return f'{lhex(self.get_absolute_offset())}\t{nname}\t{ntype}\tCategory'


class MemoryCategoryNode(anytree.NodeMixin):
    def __init__(self, name, node_type, relative_offset, parent=None):
        self.name = name
        self.node_type = node_type
        self.parent = parent
        self.relative_offset = relative_offset
        self.strider = None

    def set_stride(self, first_offset, last_offset, stride):
        assert first_offset == self.relative_offset
        self.strider = roland_address_strider(first_offset, last_offset, stride, self.fqn())

    def stride(self):
        if self.strider is not None:
            return self.strider.stride
        else:
            return 0

    def get_strider(self) -> Union[None, roland_address_strider]:
        if self.is_in_a_stride():
            p = self
            while p is not None:
                if p.strider is not None:
                    return p.strider
                p = p.parent
            raise RuntimeError("Element is in a stride but we cannot find the strider somehow")
        return None

    def is_in_a_stride(self):
        """Traverse the elements to the root to identify if we are in a stride"""
        p = self.parent
        is_in_a_stride = self.strider is not None
        while p is not None:
            is_in_a_stride = is_in_a_stride or p.is_in_a_stride()
            p = p.parent
        return is_in_a_stride

    def get_relative_offset(self, stride_num=None):
        if self.is_in_a_stride():
            if stride_num is None: 
                stride_num = 0
                if self.strider is not None:
                # try to get parent stride
                    logger.debug(
                        "get_relative_offset used in a striding item %s without a stride", self.fqn()
                    )
            strider = self.get_strider()
            logger.debug("%s: Using strider %s", self.fqn(), strider)
            return strider[stride_num] + self.relative_offset
        else:
            return self.relative_offset

    def get_absolute_offset(self, stride_num=None):
        abs_offset = self.get_relative_offset(stride_num=stride_num)
        p = self.parent
        while p is not None:
            abs_offset += p.get_relative_offset()
            p = p.parent
        return abs_offset

    def fqn(self):
        if self.parent is None:
            return self.name
        else:
            return f'{self.parent.fqn()}/{self.name}'
        
    def __getitem__(self, key):
        strider = self.get_strider()
        if strider is None:
            logger.error("Trying to stride a non-strided object")
            raise IndexError("No striding here")
        return StrideProxyUtils.build_proxy(self, key)

    def __len__(self):
        strider = self.get_strider()
        if strider is None:
            logger.error("Memory Category without a stride  has no len")
            raise TypeError(
                "object of type 'MemoryCategoryNode' without a stride has no len()"
            )
        return len(strider)

    def __iter__(self):
        strider = self.get_strider()
        if strider is None:
            logger.error("Memory Category without a stride cannot be iterated")
            raise TypeError(
                "object of type 'MemoryCategoryNode' without a stride is not iterable"
            )
        for idx, _ in enumerate(strider):
            yield StrideProxyUtils.build_proxy(self, idx)

    def get(self, key):
        return TreeUtilities.resolve(self, key)

    def __repr__(self):
        return f'[+{lhex(self.get_relative_offset())}] MemoryCategoryNode<{self.node_type}>[{self.name}]{repr(self.strider) if self.strider else ""}'

    def memreport(self, with_value=False):
        if with_value:
            logger.warning("with_value not implemented yet")
        nname = self.name.center(40)
        ntype = self.node_type.ljust(10)
        return f'{lhex(self.get_absolute_offset())}\t{nname}\t{ntype}\tCategory'


class MemoryValueNode(anytree.NodeMixin):
    """Represents a value within a MemoryCategoryNode"""

    def __init__(self, name, type_definition, parent=None):
        assert isinstance(parent, MemoryCategoryNode)
        self.name = name
        self.parent = parent
        self.type_definition = type_definition

    def __repr__(self):
        return f"[+{lhex(self.get_relative_offset())}] MemoryValueNode[{self.name}]"

    def is_in_a_stride(self):
        return self.parent.is_in_a_stride()

    def get_relative_offset(self):
        return self.type_definition["first_offset_start"]

    def get_absolute_offset(self) -> int:
        return self.parent.get_absolute_offset() + self.get_relative_offset()

    def get_rq1_bytes(self):
        logger.debug("Requesting absolute offset")
        addr_bytes = self.get_absolute_offset().to_bytes(4, "big")
        data_bytes = self.get_data_length().to_bytes(4, "big")
        return addr_bytes, data_bytes

    def get_data_length(self):
        return (
            self.type_definition["last_offset_start"]
            - self.type_definition["first_offset_start"]
            + 1
        )

    def fqn(self):
        if self.parent is None:
            return self.name
        else:
            return f'{self.parent.fqn()}/{self.name}'

    def memreport(self, with_value=False):
        if with_value:
            logger.warning("with_value not implemented yet")
        nname = self.name.center(40)
        td = self.type_definition
        off_range = f"{td['first_offset_start']}__{td['last_offset_start']}"
        ntype = off_range.ljust(10)
        return f'{lhex(self.get_absolute_offset())}\t{nname}\t{ntype}\tValue'

class TreeUtilities:
    name_resolver = anytree.Resolver("name")

    @classmethod
    def resolve(cls, from_node, glob_text):
        return cls.name_resolver.glob(from_node, glob_text)


class RolandMemoryExplorer:
    def __init__(self, project_name, spec_file=None):
        self.project_name = project_name
        self.spec_file = spec_file
        self.spec_data = {}
        self.spec_tree = []
        if spec_file:
            self.load_spec()
            self.spec_to_tree()

    def load_spec(self):
        if self.spec_file is None:
            logger.warning("load_spec() called but no spec file available")
        with open(self.spec_file, "r", encoding="utf-8") as fh:
            self.spec_data = yaml.load(fh, Loader=yaml.CLoader)

    def _spectriples(self):
        return (
            self.spec_data["type_entries"],
            self.spec_data["value_entries"],
            self.spec_data["size_entries"],
        )

    def spec_to_tree(self):
        self.spec_tree = []
        types, values, sizes = self._spectriples()
        starting_point = types["ROOT"]
        root_node = MemoryCategoryNode("root", "ROOT", False, None)
        self.spec_tree.append(root_node)
        # sanity check
        assert set(values) == set(sizes)

        for elem in starting_point:
            elem_type = elem["type"]
            if elem_type in types:
                # this element has sub-elements that need processing
                # each sub-element is a container with leaf values
                # or it self contains sub-elements
                self.parse_complex_tree(elem, root_node)
            elif elem_type in sizes:
                # an element with a defined size is a container
                # that has leaf values directly
                self.build_value_set(elem, root_node)
                # this is an element that contains values

    def parse_complex_tree(self, top_element, parent_node):
        types, values, sizes = self._spectriples()
        elem_name = top_element["name"]
        elem_type = top_element["type"]
        elem_fo = top_element["first_offset_start"]
        elem_lo = top_element["last_offset_start"]
        # elem_size = sizes[elem_type]
        elem_definition = types[elem_type]
        # elem_e2e_stride = elem_lo - elem_fo

        assert elem_lo >= elem_fo
        elem_striding = elem_lo > elem_fo
        elem_stride = top_element.get("stride", 0)
        assert elem_striding and top_element["stride"] > 0 or not elem_striding

        top_element_node = MemoryCategoryNode(elem_name, elem_type, elem_fo, parent_node)
        if elem_stride > 0:
            top_element_node.set_stride(elem_fo, elem_lo, elem_stride)
        self.spec_tree.append(top_element_node)
        for defi in elem_definition:

            item_name = defi["name"]
            logger.debug("Element %s", item_name)

            item_type = defi["type"]
            if item_type in types:
                self.parse_complex_tree(defi, top_element_node)
            else:
                self.build_value_set(defi, top_element_node)
            # item_def = values[item_type]
            # val_node = MemoryValueNode(item_name, item_def, top_element_node)
            # self.spec_tree.append(val_node)
            # logger.info("%s.%s: %s (sz: %s = %s) / %s", elem_name, item_name, item_type, item_size, lhex(item_size, 4), f'striding: {elem_count} x {lhex(top_element["stride"], 6)}' if elem_striding else 'singlet')

    def build_value_set(self, top_element, parent_node):
        types, values, sizes = self._spectriples()
        elem_name = top_element["name"]
        elem_type = top_element["type"]
        elem_fo = top_element["first_offset_start"]
        elem_lo = top_element["last_offset_start"]
        elem_striding = elem_lo > elem_fo
        elem_stride = top_element.get("stride", 0)
        assert elem_striding and top_element["stride"] > 0 or not elem_striding

        elem_definition = values[elem_type]
        logger.critical("Adding %s elements under %s [%s]", len(elem_definition), elem_name, elem_type)
        element_node = MemoryCategoryNode(elem_name, elem_type, elem_fo, parent_node)
        if elem_stride > 0:
            element_node.set_stride(elem_fo, elem_lo, elem_stride)
        self.spec_tree.append(element_node)

        # logger.debug("Building value set for %")
        for defi in elem_definition:
            defi_name = defi["name"]
            if defi_name == "MDLJPX":
                raise RuntimeError("heh")
            # logger.debug("Definition element: %s", defi_name)
            val_node = MemoryValueNode(defi_name, defi, element_node)
            self.spec_tree.append(val_node)

    def __getitem__(self, key):
        return TreeUtilities.resolve(self.spec_tree[0], key)

    def get(self, key):
        return TreeUtilities.resolve(self.spec_tree[0], key)


class MidiUtilities:
    @classmethod
    def request_rq1_sysex(cls, val: MemoryValueNode, midiout, midiin, force_len=None):
        rq1_addr, rq1_val = val.get_rq1_bytes()
        rq1_len = int.from_bytes(rq1_val, "big")
        if force_len:
            rq1_len = force_len
        mesg = build_roland_rq1_message(rq1_addr, rq1_len)
        m_out = bytes(mesg)
        midiout.send_message(m_out)
        sleep(0.1)
        answer, _ = midiin.get_message()
        addr, val = get_sysex_data(SystemExclusive.from_bytes(answer))
        return addr, val

    @classmethod
    def read_string_rq1_sysex(cls, val: MemoryValueNode, strlen, midiout, midiin):
        _, val = cls.request_rq1_sysex(val, midiout, midiin, strlen)
        val_bytes = val.to_bytes(strlen, "big")
        s = "".join(chr(s) for s in val_bytes if 32 <= s <= 127)
        return s

class TreeCrossingUtilities:
    @classmethod
    def full_form_tree(cls, root: Union[MemoryCategoryNode, MemoryValueNode], node_container=None):
        if node_container is None:
            node_container = []
        
        if isinstance(root, MemoryValueNode):
            logger.warning("We should not recurse down to MemoryValueNode. Check the alg. %s", root)
            return node_container
        node_container.append(root.memreport())
        if root.strider is None:
            # singlet element
            for child in root.children:
                if isinstance(child, MemoryValueNode):
                    node_container.append(child.memreport())
                else:
                    cls.full_form_tree(child, node_container)
        else:
            for instance in root:
                for child in instance.children:
                    if isinstance(child, MemoryValueNode):
                        node_container.append(child.memreport())
                    else:
                        cls.full_form_tree(child, node_container)
        return node_container
    

if __name__ == "__main__":
    import code
    import readline
    import os.path

    memex = RolandMemoryExplorer("Jupiter-XM", "jupx.yaml")
    ((midi_in, nidi_in_port_name), (midi_out, midi_out_port_name)) = (
        build_rtmidi_inout()
    )
    midi_in.ignore_types(sysex=False)

    try:
        readline.read_history_file(os.path.expanduser("~/.jupx-memx-history"))
    except FileNotFoundError:
        pass

    code.interact(local=locals())
    readline.write_history_file(os.path.expanduser("~/.jupx-memx-history"))
