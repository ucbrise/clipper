package com.clipper.container.app.data;

public class SerializableString extends DataVector<String> {

  public SerializableString(String data) {
    super(data);
  }

  public static class Parser extends DataVectorParser<String, SerializableString> {
    @Override
    SerializableString constructDataVector(String data) {
      return null;
    }

    @Override
    DataBuffer<String> createDataBuffer() {
      return null;
    }

//    @Override
//    public List<SerializableString> parse(ByteBuffer byteBuffer, List<Integer> splits) {
//      byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
//      List<SerializableString> serializableStrings = new ArrayList<>();
//      Charset asciiCharSet = US_ASCII.defaultCharset();
//      CharBuffer iterBuffer = asciiCharSet.decode(byteBuffer);
//      CharBuffer copyBuffer = iterBuffer.duplicate();
//      int stringLength = 0;
//      while (iterBuffer.remaining() > 0) {
//        char elem = iterBuffer.get();
//        if (elem == '\0' && stringLength > 0) {
//          char[] stringChars = new char[stringLength];
//          copyBuffer.get(stringChars, 0, stringLength);
//          copyBuffer.get();
//          String data = new String(stringChars);
//          SerializableString serializableString = new SerializableString(data);
//          serializableStrings.add(serializableString);
//          stringLength = 0;
//        } else {
//          stringLength++;
//        }
//      }
//      return serializableStrings;
//    }
  }
}
