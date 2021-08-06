import rdtest
import struct
import renderdoc as rd


class D3D11_Discard_Zoo(rdtest.Discard_Zoo):
    demos_test_name = 'D3D11_Discard_Zoo'
    internal = False

    def __init__(self):
        rdtest.Discard_Zoo.__init__(self)

    def check_capture(self):
        self.check_textures()

        action = self.find_action("TestStart")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, True)

        # Check the buffer
        for res in self.controller.GetResources():
            if res.name == "Buffer" or res.name == "BufferSRV" or res.name == "BufferRTV":
                data: bytes = self.controller.GetBufferData(res.resourceId, 0, 0)

                self.check(all([b == 0x88 for b in data]))

        action = self.find_action("TestEnd")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, True)

        # Check the buffers
        for res in self.controller.GetResources():
            if res.name == "Buffer" or res.name == "BufferSRV":
                data: bytes = self.controller.GetBufferData(res.resourceId, 0, 0)

                data_u32 = struct.unpack_from('=256L', data, 0)

                self.check(all([u == 0xD15CAD3D for u in data_u32]))
            elif res.name == "BufferRTV":
                data: bytes = self.controller.GetBufferData(res.resourceId, 0, 0)

                data_u32 = struct.unpack_from('=18L', data, 50)

                self.check(all([u == 0xD15CAD3D for u in data_u32]))

                data_u16 = struct.unpack_from('=H', data, 50+72)

                self.check(data_u16[0] == 0xAD3D)

                self.check(all([b == 0x88 for b in data[0:50]]))
                self.check(all([b == 0x88 for b in data[50+75:-1]]))