package com.rusefi.output;

import com.rusefi.ReaderState;
import com.rusefi.util.LazyFile;
import com.rusefi.util.LazyFileImpl;

import java.io.IOException;

/**
 * @see DataLogConsumer
 */
public class SdCardFieldsConsumer implements ConfigurationConsumer {

    private final SdCardFieldsContent content = new SdCardFieldsContent();
    private final LazyFile output;

    public SdCardFieldsConsumer(String outputFileName) {
        output = new LazyFileImpl(outputFileName);
    }

    @Override
    public void endFile() throws IOException {
        wrapContent(output, getBody());
        output.close();
    }

    public static void wrapContent(LazyFile output, String content) {
        output.write("static constexpr LogField fields[] = {\r\n" +
                "{packedTime, GAUGE_NAME_TIME, \"sec\", 0},\n");
        output.write(content);
        output.write("};\r\n");
    }

    @Override
    public void handleEndStruct(ReaderState state, ConfigStructure structure) throws IOException {
        content.handleEndStruct(state, structure);
    }

    public String getBody() {
        return content.getBody();
    }
}
