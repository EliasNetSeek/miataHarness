package com.rusefi.ldmp;

import com.devexperts.logging.Logging;
import com.rusefi.*;
import com.rusefi.output.*;
import com.rusefi.util.LazyFile;
import org.yaml.snakeyaml.Yaml;

import java.io.*;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

public class LiveDataProcessor {
    private final static Logging log = Logging.getLogging(LiveDataProcessor.class);

    private final static String header = "// generated by gen_live_documentation.sh / LiveDataProcessor.java\n";

    private final static String enumContentFileName = "console/binary/generated/live_data_ids.h";

    private final static String tsOutputsDestination = "console/binary/";

    private final ReaderProvider readerProvider;
    private final LazyFile.LazyFileFactory fileFactory;

    private final GaugeConsumer gaugeConsumer = new GaugeConsumer(tsOutputsDestination + File.separator + "generated/gauges.ini");

    private final StringBuilder enumContent = new StringBuilder(header +
            "#pragma once\n" +
            "\n" +
            "typedef enum {\n");

    private final StringBuilder baseAddressCHeader = new StringBuilder();

    private final StringBuilder totalSensors = new StringBuilder();

    private final StringBuilder fancyNewStuff = new StringBuilder();

    public final StateDictionaryGenerator stateDictionaryGenerator;

    private final StringBuilder fancyNewMenu = new StringBuilder();

    private final StringBuilder fragmentsContent = new StringBuilder(header);

    private final String extraPrepend = System.getProperty("LiveDataProcessor.extra_prepend");

    public LiveDataProcessor(String yamlFileName, ReaderProvider readerProvider, LazyFile.LazyFileFactory fileFactory) {
        this.readerProvider = readerProvider;
        this.fileFactory = fileFactory;
        stateDictionaryGenerator = new StateDictionaryGenerator(yamlFileName);
    }

    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            System.err.println("One parameter expected: name of live data yaml input file");
            System.exit(-1);
        }
        TriggerMetaGenerator.main(null);
        String yamlFileName = args[0];
        Map<String, Object> data = getStringObjectMap(new FileReader(yamlFileName));


        LiveDataProcessor liveDataProcessor = new LiveDataProcessor(yamlFileName, ReaderProvider.REAL, LazyFile.REAL);

        int sensorTsPosition = liveDataProcessor.handleYaml(data);
        liveDataProcessor.writeFiles();

        log.info("TS_TOTAL_OUTPUT_SIZE=" + sensorTsPosition);
        try (FileWriter fw = new FileWriter("console/binary/generated/total_live_data_generated.h")) {
            fw.write(header);
            fw.write("#define TS_TOTAL_OUTPUT_SIZE " + sensorTsPosition);
        }

        try (FileWriter fw = new FileWriter("console/binary/generated/sensors.java")) {
            fw.write(liveDataProcessor.totalSensors.toString());
        }

        try (FileWriter fw = new FileWriter("console/binary/generated/fancy_content.ini")) {
            fw.write(liveDataProcessor.fancyNewStuff.toString());
        }

        try (FileWriter fw = new FileWriter("console/binary/generated/fancy_menu.ini")) {
            fw.write(liveDataProcessor.fancyNewMenu.toString());
        }
        liveDataProcessor.end();
    }

    public static Map<String, Object> getStringObjectMap(Reader reader) {
        Yaml yaml = new Yaml();
        return yaml.load(reader);
    }

    private void end() throws IOException {
        gaugeConsumer.endFile();
    }

    interface EntryHandler {
        void onEntry(String name, String javaName, String folder, String prepend, boolean withCDefines, String[] outputNames, String constexpr, String conditional, String engineModule, Boolean isPtr, String cppFileName) throws IOException;
    }

    public int handleYaml(Map<String, Object> data) throws IOException {
        JavaSensorsConsumer javaSensorsConsumer = new JavaSensorsConsumer();

        OutputsSectionConsumer outputsSections = new OutputsSectionConsumer(tsOutputsDestination + File.separator + "generated/output_channels.ini");

        ConfigurationConsumer dataLogConsumer = new DataLogConsumer(tsOutputsDestination + File.separator + "generated/data_logs.ini");

        SdCardFieldsContent sdCardFieldsConsumer = new SdCardFieldsContent();

        GetOutputValueConsumer outputValueConsumer = new GetOutputValueConsumer("controllers/lua/generated/output_lookup_generated.cpp", fileFactory);

        EntryHandler handler = new EntryHandler() {
            @Override
            public void onEntry(String name, String javaName, String folder, String prepend, boolean withCDefines, String[] outputNames, String constexpr, String conditional, String engineModule, Boolean isPtr, String cppFileName) throws IOException {
                // TODO: use outputNames

                stateDictionaryGenerator.onEntry(name, javaName, folder, prepend, withCDefines, outputNames, constexpr, conditional, engineModule, isPtr, cppFileName);

                int startingPosition = javaSensorsConsumer.sensorTsPosition;
                log.info("Starting " + name + " at " + startingPosition + " with [" + conditional + "]");

                baseAddressCHeader.append("#define " + name.toUpperCase() + "_BASE_ADDRESS " + startingPosition + "\n");

                ReaderState state = new ReaderStateImpl(readerProvider, fileFactory);
                state.setDefinitionInputFile(folder + File.separator + name + ".txt");
                state.setWithC_Defines(withCDefines);

                state.addDestination(javaSensorsConsumer,
                        outputsSections,
                        dataLogConsumer
                );
                FragmentDialogConsumer fragmentDialogConsumer = new FragmentDialogConsumer(name);
                state.addDestination(fragmentDialogConsumer);

                if (extraPrepend != null)
                    state.addPrepend(extraPrepend);
                state.addPrepend(prepend);
                state.addCHeaderDestination(folder + File.separator + name + "_generated.h");

                int baseOffset = outputsSections.getBaseOffset();
                state.addDestination(new FileJavaFieldsConsumer(state, "../java_console/models/src/main/java/com/rusefi/config/generated/" + javaName, baseOffset, fileFactory));

                if (constexpr != null) {
                    sdCardFieldsConsumer.home = constexpr;
                    sdCardFieldsConsumer.conditional = conditional;
                    sdCardFieldsConsumer.isPtr = isPtr;
                    state.addDestination((state1, structure) -> sdCardFieldsConsumer.handleEndStruct(state1, structure));

                    outputValueConsumer.currentSectionPrefix = constexpr;
                    outputValueConsumer.moduleMode = false;
                    outputValueConsumer.conditional = conditional;
                    state.addDestination((state1, structure) -> outputValueConsumer.handleEndStruct(state1, structure));

                } else if (engineModule != null) {
                    outputValueConsumer.currentEngineModule = engineModule;
                    outputValueConsumer.moduleMode = true;
                    outputValueConsumer.conditional = conditional;
                    outputValueConsumer.isPtr = isPtr;
                    state.addDestination((state1, structure) -> outputValueConsumer.handleEndStruct(state1, structure));
                }
                state.addDestination(new ConfigurationConsumer() {
                    @Override
                    public void handleEndStruct(ReaderState readerState, ConfigStructure structure) throws IOException {
                        gaugeConsumer.handleEndStruct(readerState, structure);
                    }
                });

                state.doJob();

                fancyNewStuff.append(fragmentDialogConsumer.getContent());

                fancyNewMenu.append(fragmentDialogConsumer.menuLine());

                log.info("Done with " + name + " at " + javaSensorsConsumer.sensorTsPosition);
            }
        };


        ArrayList<LinkedHashMap> liveDocs = (ArrayList<LinkedHashMap>) data.get("Usages");

        for (LinkedHashMap entry : liveDocs) {
            String name = (String) entry.get("name");
            String java = (String) entry.get("java");
            String folder = (String) entry.get("folder");
            String prepend = (String) entry.get("prepend");
            String constexpr = (String) entry.get("constexpr");
            String engineModule = (String) entry.get("engineModule");
            String cppFileName = (String) entry.get("cppFileName");
            if (cppFileName == null)
                cppFileName = name;
            String conditional = (String) entry.get("conditional_compilation");
            Boolean withCDefines = (Boolean) entry.get("withCDefines");
            Boolean isPtr = (Boolean) entry.get("isPtr");
            // Defaults to false if not specified
            withCDefines = withCDefines != null && withCDefines;
            isPtr = isPtr != null && isPtr;

            Object outputNames = entry.get("output_name");

            String[] outputNamesArr;
            if (outputNames == null) {
                outputNamesArr = new String[0];
            } else if (outputNames instanceof String) {
                outputNamesArr = new String[1];
                outputNamesArr[0] = (String) outputNames;
            } else {
                ArrayList<String> nameList = (ArrayList<String>) outputNames;
                outputNamesArr = new String[nameList.size()];
                nameList.toArray(outputNamesArr);
            }

            handler.onEntry(name, java, folder, prepend, withCDefines, outputNamesArr, constexpr, conditional, engineModule, isPtr, cppFileName);

            String enumName = "LDS_" + name;
            String type = name + "_s"; // convention
            enumContent.append(enumName + ",\n");

            if (outputNamesArr.length < 2) {
                fragmentsContent
                        .append("decl_frag<")
                        .append(type)
                        .append(">{},\n");
            } else {
                for (int i = 0; i < outputNamesArr.length; i++) {
                    if (i != 0) {
                        // TODO: remove once the rest of the handling for multiple copies of one struct is in place.
                        fragmentsContent.append("// ");
                    }

                    fragmentsContent
                            .append("decl_frag<")
                            .append(type)
                            .append(", ")
                            .append(i)
                            .append(">{},\t// ")
                            .append(outputNamesArr[i])
                            .append("\n");
                }
            }
        }
        enumContent.append("} live_data_e;\n");

        LazyFile lazyFile = fileFactory.create("console/binary_log/log_fields_generated.h");
        SdCardFieldsConsumer.wrapContent(lazyFile, sdCardFieldsConsumer.getBody());
        lazyFile.close();

        outputValueConsumer.endFile();

        GetConfigValueConsumer.writeStringToFile("../java_console/io/src/main/java/com/rusefi/enums/StateDictionaryFactory.java", stateDictionaryGenerator.getCompleteClass(), fileFactory);

        totalSensors.append(javaSensorsConsumer.getContent());

        return javaSensorsConsumer.sensorTsPosition;
    }

    private void writeFiles() throws IOException {
        try (FileWriter fw = new FileWriter(enumContentFileName)) {
            fw.write(enumContent.toString());
            fw.write(baseAddressCHeader.toString());
        }

        try (FileWriter fw = new FileWriter("console/binary/generated/live_data_fragments.h")) {
            fw.write(fragmentsContent.toString());
        }
    }
}
