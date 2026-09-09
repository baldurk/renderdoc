from typing import Callable

import renderdoc as rd
import rdtest


class VK_Annotations(rdtest.Annotations):
    demos_test_name = 'VK_Annotations'
    internal = False

    def check_capture(self):
        super().check_resource_annotations()
        super().check_command_annotations(True)

        annot: Callable[[str], rd.SDObject | None] = lambda x: annots.FindChildByKeyPath(x)

        # Check annotations attached to indirect draws
        draw_indirect_count = self.find_action("DrawIndirectCount")
        with rdtest.log.auto_section('DrawIndirectCount'):
            expected_values = [("Start", 10000),
                               ("Initial", 20000),
                               ("Pre-DrawIndirectCount", 30000)]
            if draw_indirect_count is not None:
                expected_values += [
                    ("vkCmdDrawIndirectCount(0)", 1),
                    ("vkCmdDrawIndirectCount(<0>)", 2),
                    ("Indirect sub-command[0](<3, 1>)", 3),
                    ("Indirect sub-command[1](<3, 1>)", 3)]
            expected_values += [("Post-DrawIndirectCount", 40000)]

            action = self.get_first_action()
            for name, value in expected_values:
                action = self.find_action(name, action.eventId)
                annots = action.events[-1].annotations
                key = "draw.indirect"
                self.check_eq(annot(key).type.basetype, rd.SDBasic.SignedInteger)
                self.check_eq(annot(key).AsInt(), value)

        # Check loose event annotation attached to a barrier
        with rdtest.log.auto_section('Loose Event'):
            marker = self.find_action("Loose")
            action = marker.nextAction
            annots = action.events[0].annotations
            self.check_eq(annot("loose.int").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("loose.int").AsInt(), 1)

        # Check loose event annotation attached to vkEndCommandBuffer
        with rdtest.log.auto_section('vkEndCommandBuffer Event'):
            marker = self.find_action("Loose")
            action = marker.nextAction
            annots = action.events[-1].annotations
            self.check_eq(annot("loose.int").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("loose.int").AsInt(), 2)
            assert annot("new.value") is None

        # Check loose event annotation in an empty command buffer
        with rdtest.log.auto_section('Empty Command Buffer'):
            marker = self.find_action("Empty")
            action = marker.nextAction
            annots = action.events[0].annotations
            self.check_eq(annot("empty.int").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("empty.int").AsInt(), 1)
