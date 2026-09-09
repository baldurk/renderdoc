from __future__ import annotations
from typing import Callable

import renderdoc as rd
import rdtest


class D3D12_Annotations(rdtest.Annotations):
    demos_test_name = 'D3D12_Annotations'
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
                    ("ExecuteIndirect(maxCount 0, count <0>)", 1),
                    ("ExecuteIndirect(maxCount 10, count <0>)", 2),
                    ("[0] arg0: IndirectDraw(<3, 1>)", 3),
                    ("[1] arg0: IndirectDraw(<3, 2>)", 3)]
            expected_values += [("Post-DrawIndirectCount", 40000)]

            action = self.get_first_action()
            for name, value in expected_values:
                action = self.find_action(name, action.eventId)
                rdtest.log.print(f'Checking {name}')
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
        with rdtest.log.auto_section('CommandList Close Event'):
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
