package data;

import sun.nio.cs.US_ASCII;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

public class SerializableString extends DataVector<String> {

    public SerializableString(String data) {
        super(data);
    }

    @Override
    public byte[] toBytes() {
        String nullTerminatedData = data + '\0';
        return nullTerminatedData.getBytes(StandardCharsets.US_ASCII);
    }

    public static class Parser extends DataVectorParser<String, SerializableString> {

        @Override
        SerializableString constructDataVector(String data) {
            return null;
        }

        @Override
        DataBuffer<String> getDataBuffer() { return null; }

        @Override
        public List<SerializableString> parse(ByteBuffer byteBuffer, List<Integer> splits) {
            List<SerializableString> serializableStrings = new ArrayList<>();
            Charset asciiCharSet = US_ASCII.defaultCharset();
            CharBuffer iterBuffer = asciiCharSet.decode(byteBuffer);
            CharBuffer copyBuffer = iterBuffer.duplicate();
            int stringLength = 0;
            while(iterBuffer.remaining() > 0) {
                char elem = iterBuffer.get();
                if(elem == '\0' && stringLength > 0) {
                    char[] stringChars = new char[stringLength];
                    copyBuffer.get(stringChars, 0, stringLength);
                    copyBuffer.get();
                    String data = new String(stringChars);
                    SerializableString serializableString = new SerializableString(data);
                    serializableStrings.add(serializableString);
                    stringLength = 0;
                } else {
                    stringLength++;
                }
            }
            return serializableStrings;
        }
    }
}
