from __future__ import annotations
from typing import Callable

import renderdoc as rd
import rdtest


# Not a direct test, re-used by API-specific tests
class Annotations(rdtest.TestCase):
    internal = True

    def check_resource_annotations(self):
        annot: Callable[[str], rd.SDObject | None] = lambda x: annots.FindChildByKeyPath(x)

        with rdtest.log.auto_section('Resource annotations'):
            res = self.get_resource_by_name('Annotated Image')

            annots = res.annotations
            assert annots is not None

            self.check_eq(annot("basic.bool").type.basetype, rd.SDBasic.Boolean)
            self.check_eq(annot("basic.bool").AsBool(), True)
            
            self.check_eq(annot("basic.int32").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("basic.int32").type.byteSize, 4)
            self.check_eq(annot("basic.int32").AsInt(), -3)
            
            self.check_eq(annot("basic.int64").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("basic.int64").type.byteSize, 8)
            self.check_eq(annot("basic.int64").AsInt(), -3000000000000)
            
            self.check_eq(annot("basic.uint32").type.basetype, rd.SDBasic.UnsignedInteger)
            self.check_eq(annot("basic.uint32").type.byteSize, 4)
            self.check_eq(annot("basic.uint32").AsInt(), 3)
            
            self.check_eq(annot("basic.uint64").type.basetype, rd.SDBasic.UnsignedInteger)
            self.check_eq(annot("basic.uint64").type.byteSize, 8)
            self.check_eq(annot("basic.uint64").AsInt(), 3000000000000)

            self.check_eq(annot("basic.float").type.basetype, rd.SDBasic.Float)
            self.check_eq(annot("basic.float").type.byteSize, 4)
            self.check_eq(annot("basic.float").AsFloat(), 3.25)
            
            self.check_eq(annot("basic.double").type.basetype, rd.SDBasic.Float)
            self.check_eq(annot("basic.double").type.byteSize, 8)
            assert rdtest.value_compare(annot("basic.double").AsFloat(), 3.25000000001)

            self.check_eq(annot("basic.string").type.basetype, rd.SDBasic.String)
            self.check_eq(annot("basic.string").AsString(), "Hello, World!")

            self.check_eq(annot("basic.object").type.basetype, rd.SDBasic.Resource)
            obj = self.get_resource(annot("basic.object").AsResourceId())
            self.check_eq(obj.name, "Vertex Buffer")
            
            self.check_eq(annot("basic.object.__offset").AsInt(), 64)
            self.check_eq(annot("basic.object.__size").AsInt(), 32)
            
            assert rdtest.value_compare(annot("basic.vec3.1").AsFloat(), 1.1)
            assert rdtest.value_compare(annot("basic.vec3.2").AsFloat(), 2.2)
            assert rdtest.value_compare(annot("basic.vec3.3").AsFloat(), 3.3)
            
            self.check_eq(annot("deep.nested.path.to.annotation").AsInt(), -4)
            self.check_eq(annot("deep.nested.path.to.annotation2").AsInt(), -5)
            self.check_eq(annot("deep.alternate.path.to.annotation").AsInt(), -6)
            
            assert annot("deleteme") is None
            
            assert annot("path.deleted.by.parent") is None
            assert annot("path.deleted.by.parent2") is None
            assert annot("path.deleted") is None

    def check_command_annotations(self, cmd_buffers: bool):
        annot: Callable[[str], rd.SDObject | None] = lambda x: annots.FindChildByKeyPath(x)

        with rdtest.log.auto_section('Event annotations'):
            action = self.find_action("Start")
            rdtest.log.print(f"Checking {action.customName}")

            annots = action.events[-1].annotations
            assert annots is not None

            # Should not have this annotation, it happened prior to the capture
            assert annot("queue.too_old") is None

            # normal value set on the queue
            self.check_eq(annot("queue.value").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("queue.value").AsInt(), 1000)

            # this will later be overwritten by the commands, but for now has the queue value
            self.check_eq(annot("command.overwritten").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.overwritten").AsInt(), 9999)

            # this is inherited by the commands even as other siblings are modified
            self.check_eq(annot("command.inherited").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.inherited").AsInt(), 1234)

            # this will be deleted
            self.check_eq(annot("command.deleted").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.deleted").AsInt(), 50)

            action = self.find_action("Initial")
            rdtest.log.print(f"Checking {action.customName}")

            annots = action.events[-1].annotations
            assert annots is not None

            # normal value set on the queue
            self.check_eq(annot("queue.value").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("queue.value").AsInt(), 1000)

            # this has the overwritten value now
            self.check_eq(annot("command.overwritten").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.overwritten").AsInt(), -3333)

            # this is inherited by the commands even as other siblings are modified
            self.check_eq(annot("command.inherited").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.inherited").AsInt(), 1234)
            
            # this has now been deleted
            assert annot("command.deleted") is None

            # this is a new command-local value
            self.check_eq(annot("command.new").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.new").AsInt(), 3333)

            # this is a new command-local value
            self.check_eq(annot("new.value").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("new.value").AsInt(), 2000)

            action = self.find_action("Pre-Draw")
            rdtest.log.print(f"Checking {action.customName}")

            annots = action.events[-1].annotations
            assert annots is not None

            # normal value set on the queue
            self.check_eq(annot("queue.value").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("queue.value").AsInt(), 1000)

            # this has the overwritten value now
            self.check_eq(annot("command.overwritten").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.overwritten").AsInt(), -3333)

            # this is inherited by the commands even as other siblings are modified
            self.check_eq(annot("command.inherited").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.inherited").AsInt(), 1234)
            
            # this has now been deleted
            assert annot("command.deleted") is None

            # this is a command-local value, which has changed type
            self.check_eq(annot("command.new").type.basetype, rd.SDBasic.Float)
            self.check_eq(annot("command.new").AsFloat(), 1.75)

            # this value has't changed
            self.check_eq(annot("new.value").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("new.value").AsInt(), 2000)

            predraw_id = action.eventId

            action = self.find_action("Draw 1")
            rdtest.log.print(f"Checking {action.customName}")

            annots = action.events[-1].annotations
            assert annots is not None

            # should be no events between these
            self.check_eq(predraw_id + 1, action.eventId)

            # this value was deleted and re-added, so it should be present as the new value
            self.check_eq(annot("new.value").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("new.value").AsInt(), 4000)

            # normal value set on the queue
            self.check_eq(annot("queue.value").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("queue.value").AsInt(), 1000)

            # this has the overwritten value now
            self.check_eq(annot("command.overwritten").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.overwritten").AsInt(), -3333)

            # this is inherited by the commands even as other siblings are modified
            self.check_eq(annot("command.inherited").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.inherited").AsInt(), 1234)
            
            # this has now been deleted
            assert annot("command.deleted") is None

            # this is a command-local value, which has changed type
            self.check_eq(annot("command.new").type.basetype, rd.SDBasic.Float)
            self.check_eq(annot("command.new").AsFloat(), 1.75)

            self.check_eq(annot("new.value").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("new.value").AsInt(), 4000)

            action = self.find_action("Draw 2")
            rdtest.log.print(f"Checking {action.customName}")

            annots = action.events[-1].annotations
            assert annots is not None

            # all of the values should still be present

            self.check_eq(annot("queue.value").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("queue.value").AsInt(), 1000)

            self.check_eq(annot("command.overwritten").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.overwritten").AsInt(), -3333)

            self.check_eq(annot("command.inherited").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.inherited").AsInt(), 1234)
            
            assert annot("command.deleted") is None

            self.check_eq(annot("command.new").type.basetype, rd.SDBasic.Float)
            self.check_eq(annot("command.new").AsFloat(), 1.75)
            
            self.check_eq(annot("new.value").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("new.value").AsInt(), 4000)

            action = self.get_last_action()
            rdtest.log.print(f"Checking {action.customName}")
            
            annots = action.events[-1].annotations
            assert annots is not None

            # normal value set on the queue
            self.check_eq(annot("queue.value").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("queue.value").AsInt(), 1000)

            self.check_eq(annot("command.inherited").type.basetype, rd.SDBasic.SignedInteger)
            self.check_eq(annot("command.inherited").AsInt(), 1234)

            # if we have real command buffers, the annotations revert back to where they were on queues
            if cmd_buffers:
                self.check_eq(annot("command.overwritten").type.basetype, rd.SDBasic.SignedInteger)
                self.check_eq(annot("command.overwritten").AsInt(), 9999)

                self.check_eq(annot("command.deleted").type.basetype, rd.SDBasic.SignedInteger)
                self.check_eq(annot("command.deleted").AsInt(), 50)

                assert annot("command.new") is None
                assert annot("new.value") is None
            # otherwise they will be the same as on the command buffer
            else:
                self.check_eq(annot("command.overwritten").type.basetype, rd.SDBasic.SignedInteger)
                self.check_eq(annot("command.overwritten").AsInt(), -3333)
                
                assert annot("command.deleted") is None
                
                self.check_eq(annot("command.new").type.basetype, rd.SDBasic.Float)
                self.check_eq(annot("command.new").AsFloat(), 1.75)
                
                self.check_eq(annot("new.value").type.basetype, rd.SDBasic.SignedInteger)
                self.check_eq(annot("new.value").AsInt(), 4000)
