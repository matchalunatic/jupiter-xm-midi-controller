#!/usr/bin/env python
"""This is meant to run on a computer. It may also run on a microcontroller but
no warranty
"""
import code
# import jupiterlib
import yaml
import anytree

#import rtmidi
#import rtmidi.midiutil
from anytree import NodeMixin, RenderTree

_jupxspec = None
def load_jupx_spec():
    global _jupxspec
    if _jupxspec is not None:
        return _jupxspec
    with open('jupx.yaml', 'r', encoding='utf-8') as fh:
        _jupxspec = yaml.load(fh, Loader=yaml.CLoader)
        return _jupxspec

class MemoryZone(NodeMixin):
    def __init__(self, zone_name, parent_zone, start_offset, item_size, item_count):
        self.name = zone_name
        self.parent = parent_zone
        self.zone_size = item_size * item_count
        self.relative_offset = start_offset
        self.item_count = item_count

    @property
    def absolute_offset(self):
        if self.parent is None:
            return self.relative_offset
        return self.parent.absolute_offset + self.relative_offset

    @property
    def full_qualified_name(self):
        if self.parent is None:
            return self.name
        else:
            return f'{self.parent.full_qualified_name}/{self.name}'

class MemorySpace:
    def __init__(self, total_size, name):
        """Create a MemorySpace with specified total size and gives it a name"""
        self.name = name
        
        self.total_size = total_size
        # self.memory_repr = bytearray(total_size)
        # self.zone_index = [None] * total_size
        self.node_tree = [
            MemoryZone("RootNode", None, 0, total_size, 1)
        ]
        self.node_index = {
            'root': self.node_tree[0],
        }
        self.dubious_memory_zones = []

    def add_singular_zone(self, offset, zone_name, zone_size, parent_zone='root', zone_size_certain=True):
        mz = MemoryZone(zone_name, self.node_index[parent_zone], offset, zone_size, 1)
        self.node_tree.append(mz)
        self.node_index[mz.full_qualified_name] = mz
        if not zone_size_certain:
            self.dubious_memory_zones.append((mz.full_qualified_name, offset))

    def add_zone(self, offset, zone_name, item_size, item_count, parent_zone='root'):
        if item_count == 1:
            # no repeating markers
            return self.add_singular_zone(offset, zone_name, item_size)
        mz = MemoryZone(zone_name, self.node_index[parent_zone], offset, item_size, item_count)
        self.node_tree.append(mz)
        self.node_index[mz.full_qualified_name] = mz

def build_rtmidi_inout():
    available_in = rtmidi.MidiIn().get_ports()
    available_out= rtmidi.MidiOut().get_ports()
    (jupi_in,) = (a for a in available_in if a.startswith('JUPITER-X') and 'DAW CTRL' not in a and 'Jupiter X Controller' not in a)
    (jupi_out,) = (a for a in available_out if a.startswith('JUPITER-X') and 'DAW CTRL' not in a and 'Jupiter X Controller' not in a)
    return rtmidi.midiutil.open_midiinput(jupi_in), rtmidi.midiutil.open_midioutput(jupi_out)

def map_out_jupiter_x_parameter_addresses(map: MemorySpace):
    """Take the spec and outputs an object that can map an
    entire address space saying which part corresponds to what"""
    jupx_spec = load_jupx_spec()
    root_type_entries = jupx_spec['type_entries']['ROOT']
    value_entries = jupx_spec['value_entries']
    for val in root_type_entries:
        if val['first_offset_start'] == val['last_offset_start']:
            map.add_singular_zone(val['first_offset_start'], val['name'], 65536, 'root', False)
        else:
            item_count = (val['last_offset_start'] - val['first_offset_start']) / val['stride']
            map.add_zone(val['first_offset_start'], val['name'], val['stride'], item_count, 'root')
    return map

if __name__ == '__main__':
    import sys
    if len(sys.argv) > 1 and sys.argv[1].startswith('sh'):
        (inport, in_name), (outport, out_name) = build_rtmidi_inout()
        jupx_spec = load_jupx_spec()

        code.interact(local=locals())
    else:
        map_jupiter_xm = MemorySpace(0x7fffffff, "JupiterXMMemory")

        m = map_out_jupiter_x_parameter_addresses(map_jupiter_xm)
        print(m)
        # code.interact(local=locals())
