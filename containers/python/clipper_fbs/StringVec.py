# automatically generated by the FlatBuffers compiler, do not modify

# namespace: clipper

import flatbuffers

class StringVec(object):
    __slots__ = ['_tab']

    @classmethod
    def GetRootAsStringVec(cls, buf, offset):
        n = flatbuffers.encode.Get(flatbuffers.packer.uoffset, buf, offset)
        x = StringVec()
        x.Init(buf, n + offset)
        return x

    # StringVec
    def Init(self, buf, pos):
        self._tab = flatbuffers.table.Table(buf, pos)

    # StringVec
    def Data(self, j):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        if o != 0:
            a = self._tab.Vector(o)
            return self._tab.String(a + flatbuffers.number_types.UOffsetTFlags.py_type(j * 4))
        return ""

    # StringVec
    def DataLength(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        if o != 0:
            return self._tab.VectorLen(o)
        return 0

def StringVecStart(builder): builder.StartObject(1)
def StringVecAddData(builder, data): builder.PrependUOffsetTRelativeSlot(0, flatbuffers.number_types.UOffsetTFlags.py_type(data), 0)
def StringVecStartDataVector(builder, numElems): return builder.StartVector(4, numElems, 4)
def StringVecEnd(builder): return builder.EndObject()
