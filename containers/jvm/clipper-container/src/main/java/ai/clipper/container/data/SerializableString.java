package ai.clipper.container.data;

import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.nio.charset.CharsetEncoder;
import java.nio.charset.StandardCharsets;

public class SerializableString extends DataVector<String> {
  private static int MAXIMUM_UTF_8_CHAR_LENGTH_BYTES = 4;

  public SerializableString(String data) {
    super(data);
  }

  /**
   * Writes the byte representation of the UTF-8
   * encoded string to the provided buffer
   *
   * @return The number of bytes written to the buffer
   */
  public int encodeUTF8ToBuffer(ByteBuffer buffer) {
    CharBuffer stringContent = CharBuffer.wrap(data);
    Charset UTFCharset = StandardCharsets.UTF_8;
    CharsetEncoder encoder = UTFCharset.newEncoder();
    int startPosition = buffer.position();
    encoder.encode(stringContent, buffer, true);
    int endPosition = buffer.position();
    return (endPosition - startPosition);
  }

  /**
   * @return The maximum size of the utf-8 encoded string
   * in bytes. The actual byte size of a fixed-length string
   * may vary depending on its character content.
   */
  public int maxSizeBytes() {
    return data.length() * MAXIMUM_UTF_8_CHAR_LENGTH_BYTES;
  }

  public static class Parser extends DataVectorParser<String, SerializableString> {
    @Override
    public SerializableString constructDataVector(ByteBuffer data, long byteSize) {
      data.limit((int) byteSize);
      String content = StandardCharsets.UTF_8.decode(data).toString();
      return new SerializableString(content);
    }
  }
}
