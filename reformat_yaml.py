"""Reformat YAML so it makes more sense

Rules:
- By default, lists are on many lines
- If a list is made of basic types and there are more than 3, then use flow style
"""
from ruamel.yaml import YAML
from ruamel.yaml.representer import SafeRepresenter, BaseRepresenter


_sr = SafeRepresenter()


def represent_list(representer, data):
    if len(data) > 3 and isinstance (data[0], (int, str, float)):
        return SafeRepresenter.represent_sequence(representer, 'tag:yaml.org,2002:seq', data, flow_style=True)
    else:
        return SafeRepresenter.represent_sequence(representer, 'tag:yaml.org,2002:seq', data, flow_style=False)


def reformat_yaml(infile: str, outfile: str) -> bool:
    SafeRepresenter.add_representer(list, represent_list)
    r = YAML(typ='safe')
    r.sort_base_mapping_type_on_output = False
    r.default_flow_style = False

    with open(infile, 'r', encoding='utf-8') as fh:
        with open(outfile, 'w', encoding='utf-8') as fi:
            r.dump(r.load(fh), fi)
            return True
        

if __name__ == '__main__':
    import sys
    reformat_yaml(sys.argv[1], sys.argv[2])