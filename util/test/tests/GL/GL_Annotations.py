import rdtest


class GL_Annotations(rdtest.Annotations):
    demos_test_name = 'GL_Annotations'
    internal = False

    def check_capture(self):
        super().check_resource_annotations()
        super().check_command_annotations(False)
