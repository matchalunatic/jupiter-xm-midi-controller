from memexplorer import RolandMemoryExplorer


def test_memex():
    memex = RolandMemoryExplorer("Jupiter-XM", "jupx.yaml")
    (master_ks,) = memex["System/Master Key Shift"]
    assert master_ks.get_absolute_offset() == 4
    assert master_ks.get_relative_offset() == 4
    (scenes,) = memex["./User Scene"]
    assert scenes[-1].get("NAME 1")[0].get_absolute_offset() == 0x49_7B_00_00
    assert scenes[0].get("NAME 1")[0].get_absolute_offset() == 0x40_00_00_00
    assert scenes[127].get("NAME 1")[0].get_relative_offset() == 0
