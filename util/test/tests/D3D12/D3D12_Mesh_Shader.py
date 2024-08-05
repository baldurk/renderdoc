import renderdoc as rd
import rdtest
from rdtest import analyse

class D3D12_Mesh_Shader(rdtest.TestCase):
    demos_test_name = 'D3D12_Mesh_Shader'
    demos_frame_cap = 5

    def decode_task_data(self, controller: rd.ReplayController, mesh: rd.MeshFormat, payload: rd.ConstantBlock, task: int = 0):

        begin = mesh.vertexByteOffset + mesh.vertexByteStride * task
        end = min(begin + mesh.vertexByteSize, 0xffffffffffffffff)
        buffer_data = controller.GetBufferData(mesh.vertexResourceId, begin, end -begin)

        ret = []
        offset = 0
        for var in payload.variables:
            var_data = {}
            var_data[var.name] = []
            # This is not complete to decode all possible payload layouts
            for i in range(var.type.elements):
                format = rd.ResourceFormat()
                format.compByteWidth = rd.VarTypeByteSize(var.type.baseType)
                format.compCount = var.type.columns
                format.compType = rd.VarTypeCompType(var.type.baseType)
                format.type = rd.ResourceFormatType.Regular

                data =  analyse.unpack_data(format, buffer_data, offset)
                var_data[var.name] += data
                offset += format.compByteWidth * format.compCount
            ret.append(var_data)

        return ret

    def get_task_data(self, action: rd.ActionDescription):
        mesh: rd.MeshFormat = self.controller.GetPostVSData(0, 0, rd.MeshDataStage.TaskOut)
        if mesh.numIndices == 0:
            raise self.TestFailureException("Task data is empty")

        if len(mesh.taskSizes) == 0:
            raise self.TestFailureException("Task data is empty")

        pipe: rd.PipeState = self.controller.GetPipelineState()
        shader = pipe.GetShaderReflection(rd.ShaderStage.Task)
        taskIdx = 0
        task = action.dispatchDimension
        data = []
        for x in range(task[0]):
            for y in range(task[1]):
                for z in range(task[2]):
                    data += self.decode_task_data(self.controller, mesh, shader.taskPayload, taskIdx)
                    taskIdx += 1
        return data

    def build_global_taskout_reference(self):
        reference = {}
        for i in range(2):
            reference[i] = {
                'tri': (i*2,i*2+1),
            }
        return reference

    def build_local_taskout_reference(self):
        reference = {}
        reference[0] = { 'tri': [0, 1, 2, 3] }
        return reference

    def build_meshout_reference(self, orgY, color):
        countTris = 4 
        triSize = 0.2
        deltX = 0.42
        orgX = -0.65
        i = 0
        reference = {}
        for tri in range(countTris):
            for vert in range(3):
                posX = orgX + tri * deltX
                posY = orgY

                if vert == 0:
                    posX += -0.2
                    posY += -0.2
                    uv = [0.0, 0.0]
                elif vert == 1:
                    posX += 0.0
                    posY += 0.2
                    uv = [0.0, 1.0]
                elif vert == 2:
                    posX += 0.2
                    posY += -0.2
                    uv = [1.0, 0.0]

                reference[i] = {
                    'vtx': i,
                    'idx': i,
                    'SV_Position': [posX, posY, 0.0, 1.0],
                    'COLOR': color,
                    'TEXCOORD': uv
                }
                i += 1
        return reference

    def check_capture(self):
        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        action = self.find_action("Mesh Shaders")

        action = action.next
        self.controller.SetFrameEvent(action.eventId, False)
        rdtest.log.print(f"Pure Mesh Shader Test EID:{action.eventId}")
        
        orgY = 0.65
        color = [1.0, 0.0, 0.0, 1.0]
        postms_ref = self.build_meshout_reference(orgY, color)
        postms_data = self.get_postvs(action, rd.MeshDataStage.MeshOut, 0, action.numIndices)
        self.check_mesh_data(postms_ref, postms_data)

        action = action.next
        self.controller.SetFrameEvent(action.eventId, False)
        rdtest.log.print(f"Amplification Shader with Global Payload EID:{action.eventId}")

        postts_ref = self.build_global_taskout_reference()
        postts_data = self.get_task_data(action)
        self.check_task_data(postts_ref, postts_data)

        orgY = 0.0
        color = [0.0, 1.0, 0.0, 1.0]
        postms_ref = self.build_meshout_reference(orgY, color)
        postms_data = self.get_postvs(action, rd.MeshDataStage.MeshOut, 0, action.numIndices)
        self.check_mesh_data(postms_ref, postms_data)

        action = action.next
        self.controller.SetFrameEvent(action.eventId, False)
        rdtest.log.print(f"Amplification Shader with Local Payload EID:{action.eventId}")

        postts_ref = self.build_local_taskout_reference()
        postts_data = self.get_task_data(action)
        self.check_task_data(postts_ref, postts_data)

        orgY = -0.65
        color = [0.0, 0.0, 1.0, 1.0]
        postms_ref = self.build_meshout_reference(orgY, color)
        postms_data = self.get_postvs(action, rd.MeshDataStage.MeshOut, 0, action.numIndices)
        self.check_mesh_data(postms_ref, postms_data)
