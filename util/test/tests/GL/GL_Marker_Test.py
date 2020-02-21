import rdtest
import renderdoc as rd


class GL_Marker_Test(rdtest.TestCase):
    demos_test_name = 'GL_Marker_Test'

    def check_capture(self):
        draws = self.controller.GetDrawcalls()

        d = draws[1]

        names = [
            'EXT marker 1',
            'EXT marker 2',
            'EXT marker 3',
            'KHR marker 1',
            '',
            'KHR marker 3',
            'Core marker 1',
            '',
            'Core marker 3',
        ]

        for n in names:
            self.check(d.name == n)
            d = d.children[0]

        d = d.parent

        names = [
            'EXT event 1',
            'EXT event 2',
            'EXT event 3',
            'KHR event 1',
            '',
            'KHR event 3',
            'Core event 1',
            '',
            'Core event 3',
            'GREMEDY event 1',
            'GREMEDY event 2',
            'GREMEDY event 3',
        ]

        i = 0
        for name in names:
            self.check(name == d.children[i].name)
            i += 1

        self.check(i == len(names))

        self.check('glDrawArrays' in d.children[i].name)
