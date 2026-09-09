import renderdoc as rd
import rdtest


class D3D11_Annotations(rdtest.Annotations):
    demos_test_name = 'D3D11_Annotations'
    internal = False

    def check_capture(self):
        super().check_resource_annotations()
        super().check_command_annotations(False)
