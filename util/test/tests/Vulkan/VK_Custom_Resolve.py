from typing import Dict, List

import renderdoc as rd
import rdtest
import rdtest.util

class VK_Custom_Resolve(rdtest.TestCase):
    demos_test_name = 'VK_Custom_Resolve'

    def check_triangle_draw(self):
        pipe = self.controller.GetPipelineState()
        out = pipe.GetOutputTargets()[0].resource
        # centre
        green = [0.0, 1.0, 0.0, 1.0]
        self.check_pixel_value(out, 200, 150, green)

    def check_triangle_resolve(self):
        pipe = self.controller.GetPipelineState()
        out = pipe.GetOutputTargets()[0].resource
        # left triangle edge
        left = [0.0, 0.0, 1.0, 1.0]
        self.check_pixel_value(out, 150, 149, left)
        # right triangle edge
        right = [1.0, 0.0, 0.0, 1.0]
        self.check_pixel_value(out, 249, 149, right)
        # centre
        centre = [0.0, 0.25, 0.0, 1.0]
        self.check_pixel_value(out, 200, 150, centre)

    def check_resource_usage(self, markerName: str, expectedUsages: List[rd.ResourceUsage]):
        action = self.find_action(markerName)
        self.set_event(action.eventId+1, True)
        pipe = self.controller.GetPipelineState()
        out = pipe.GetOutputTargets()[0].resource
        usages = self.controller.GetUsage(out)
        if len(usages) != len(expectedUsages):
            raise rdtest.TestFailureException(f"Incorrect resource usages count expected:{len(expectedUsages)} actual:{len(usages)}")
        for i, u in enumerate(usages):
            if u.usage != expectedUsages[i]:
                raise rdtest.TestFailureException(f"EID:{u.eventId} Incorrect resource usage expected:{expectedUsages[i].name} actual:{u.usage.name}")

    def check_pixel_history(self, passed: List[bool], preModValid: List[bool], preMod: List[rdtest.VectorValue], postModValid: List[bool], postMod: List[rdtest.VectorValue]):
        pipe = self.controller.GetPipelineState()
        rt = pipe.GetOutputTargets()[0]
        tex = rt.resource
        sub = rd.Subresource()
        x = 200
        y = 150
        with self.pixel_history(tex, x, y, sub, rt.format.compType) as history:
            modifs = history.modifs
            if len(modifs) != len(passed):
                raise rdtest.TestFailureException(f"Pixel history incorrect modifications count expected:{len(passed)} actual:{len(modifs)}")
            for i, m in enumerate(modifs):
                if m.Passed() != passed[i]:
                    raise rdtest.TestFailureException(f"EID:{m.eventId} Pixel history incorrect passed expected:{passed[i]} actual:{m.Passed()}")
                if m.preMod.IsValid() != preModValid[i]:
                    raise rdtest.TestFailureException(f"EID:{m.eventId} Pixel history incorrect pre mod valid expected:{preModValid[i]} actual:{m.preMod.IsValid()}")
                if m.preMod.IsValid():
                    if not rdtest.util.value_compare(m.preMod.col.floatValue, preMod[i], eps=1.0/255.0):
                        raise rdtest.TestFailureException(f"EID:{m.eventId} Pixel history incorrect pre mod expected:{preMod[i]} actual:{m.preMod.col.floatValue}")
                if m.postMod.IsValid() != postModValid[i]:
                    raise rdtest.TestFailureException(f"EID:{m.eventId} Pixel history incorrect post mod valid expected:{postModValid[i]} actual:{m.postMod.IsValid()}")
                if m.postMod.IsValid():
                    if not rdtest.util.value_compare(m.postMod.col.floatValue, postMod[i], eps=1.0/255.0):
                        raise rdtest.TestFailureException(f"EID:{m.eventId} Pixel history incorrect post mod expected:{postMod[i]} actual:{m.postMod.col.floatValue}")
            rdtest.log.success(f"Pixel History Worked {len(modifs)} modifications found")

    def check_capture(self):
        markers = ["MSAA Draw", "MSAA Resolve"]
        msaaTargetUsages = [
            # RenderPass 
            # Clear
            rd.ResourceUsage.Barrier, 
            rd.ResourceUsage.Discard, 
            rd.ResourceUsage.Clear, 
            rd.ResourceUsage.Barrier, 
            # Draw
            rd.ResourceUsage.ColorTarget, 
            # Resolve Draw
            rd.ResourceUsage.InputTarget,
            # EndRenderPass
            rd.ResourceUsage.Discard,
            # Dynamic
            # Clear
            rd.ResourceUsage.Barrier,
            rd.ResourceUsage.Discard,
            rd.ResourceUsage.Clear,
            rd.ResourceUsage.Barrier,
            # BeginRendering
            rd.ResourceUsage.Clear,
            # Draw
            rd.ResourceUsage.ColorTarget,
            rd.ResourceUsage.Barrier,
            # Resolve Draw
            rd.ResourceUsage.InputTarget,
            rd.ResourceUsage.Barrier, 
            ]

        msaaResolveUsages = [
            # RenderPass 
            # Clear
            rd.ResourceUsage.Barrier, 
            rd.ResourceUsage.Discard, 
            rd.ResourceUsage.Clear, 
            rd.ResourceUsage.Barrier, 
            # BeginRenderPass
            rd.ResourceUsage.Discard, 
            # Resolve Draw
            rd.ResourceUsage.ResolveDst, 

            # Dynamic
            # Clear
            rd.ResourceUsage.Barrier,
            rd.ResourceUsage.Discard,
            rd.ResourceUsage.Clear,
            rd.ResourceUsage.Barrier, 
            # BeginCustomResolve
            rd.ResourceUsage.Discard,
            # Resolve Draw
            rd.ResourceUsage.ResolveDst, 
            # BlitImage
            rd.ResourceUsage.Barrier, 
            rd.ResourceUsage.ResolveSrc,
            ]
        usages: Dict[str, List[rd.ResourceUsage]] = {}
        usages["MSAA Draw"] = msaaTargetUsages
        usages["MSAA Resolve"] = msaaResolveUsages
        for marker in markers:
            with rdtest.log.auto_section(marker):
                self.check_resource_usage(marker,usages[marker])

        sections = ["RenderPass", "Dynamic"]
        for sectionName in sections:
            with rdtest.log.auto_section(sectionName):
                with rdtest.log.auto_section("MSAA Draw"):
                    action = self.find_action(sectionName) 
                    
                    assert action is not None

                    action = self.find_action("MSAA Draw", action.eventId)
        
                    assert action is not None

                    rdtest.log.print(f'MSAA Draw: {self.action_name(action)} EID:{action.eventId}')
                    self.set_event(action.eventId+1, True)
                    self.check_triangle_draw()
                    self.check_debug_pixel(200, 150)
                    # Clear : Draw
                    countMods = 2
                    # clear: 0.2,0.5,0.2,1
                    # draw: unknown
                    passed = [True, True]
                    preModValid = [True, False]
                    postModValid = [True, False]
                    preMod: List[rdtest.VectorValue] = [(0.0,0.0,0.0,0.0), (0,0,0,0)]
                    postMod: List[rdtest.VectorValue] = [(0.2,0.5,0.2,1), (0,0,0,0)]
                    if sectionName == "Dynamic":
                        # Clear : BeginRendering : Draw
                        countMods += 3 
                        # clear 0.2,0.2,0.5,1
                        # begin: rendering 0.6,0.2,0.2,1
                        # draw: 0,1,0.1
                        passed += [True, True, True]
                        preModValid += [True, True, True]
                        postModValid += [True, True, True]
                        preMod += [(0.0,0.0,0.0,0.0), (0.2,0.2,0.5,1), (0.6,0.2,0.2,1)]
                        postMod += [(0.2,0.2,0.5,1), (0.6,0.2,0.2,1), (0,1,0,1)]
                    self.check_pixel_history(passed, preModValid, preMod, postModValid, postMod)

                with rdtest.log.auto_section("MSAA Resolve"):
                    action = self.find_action(sectionName) 

                    assert action is not None

                    action = self.find_action("MSAA Resolve", action.eventId)
        
                    assert action is not None

                    rdtest.log.print(f'MSAA Resolve: {self.action_name(action)} EID:{action.eventId}')
                    self.set_event(action.eventId+1, True)
                    self.check_triangle_resolve()
                    self.check_debug_pixel(200, 150)
                    self.check_debug_pixel(150, 149)
                    self.check_debug_pixel(249, 149)
                    # Clear : Draw
                    countMods = 2
                    # clear 0.5,0,0,1
                    # draw unknown
                    passed = [True, True]
                    preModValid = [True, False]
                    postModValid = [True, False]
                    preMod = [(0.0,0.0,0.0,0.0), (0,0,0,0)]
                    postMod = [(0.5,0.0,0.0,1), (0,0,0,0)]
                    if sectionName == "Dynamic":
                        # Clear : Draw
                        countMods = 4
                        # clear 0.0,0.0,0.5,1
                        # draw: 0,0.25,0.1
                        passed += [True, True]
                        preModValid += [True, False]
                        postModValid += [True, True]
                        preMod += [(0.0,0.0,0.0,0), (0.0,0.0,0.0,0)]
                        postMod += [(0.0,0.0,0.5,1), (0,0.25,0,1)]
                    self.check_pixel_history(passed, preModValid, preMod, postModValid, postMod)
