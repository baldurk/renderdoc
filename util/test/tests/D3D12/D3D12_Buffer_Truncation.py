import rdtest
import renderdoc as rd


class D3D12_Buffer_Truncation(rdtest.Buffer_Truncation):
    demos_test_name = 'D3D12_Buffer_Truncation'
    internal = False