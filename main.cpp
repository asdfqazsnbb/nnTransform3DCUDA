/************************************************************************

    main.cpp

    ld-chroma-decoder - Colourisation filter for ld-decode
    Copyright (C) 2018-2025 Simon Inns
    Copyright (C) 2019-2022 Adam Sampson
    Copyright (C) 2021 Chad Page
    Copyright (C) 2021 Phillip Blucas

    This file is part of ld-decode-tools.

    ld-chroma-decoder is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

************************************************************************/


#include <QCoreApplication>
#include <QDebug>
#include <QtGlobal>
#include <QCommandLineParser>
#include <QThread>
#include <memory>

#include "decoderpool.h"
#include "lib/lddecodemetadata.h"
#include "lib/logging.h"

#include "comb.h"
#include "monodecoder.h"
#include "ntscdecoder.h"
#include "outputwriter.h"

int main(int argc, char *argv[])
{
    // Set 'binary mode' for stdin and stdout on Windows
    setBinaryMode();

    // Install the local debug message handler
    setDebug(true);
    qInstallMessageHandler(debugOutputHandler);

    QCoreApplication a(argc, argv);

    // Set application name and version
    QCoreApplication::setApplicationName("ld-chroma-decoder");
    QCoreApplication::setApplicationVersion(QString("ld-decode-tools - Branch: %1 / Commit: %2").arg(APP_BRANCH, APP_COMMIT));
    QCoreApplication::setOrganizationDomain("domesday86.com");

    // Set up the command line parser
    QCommandLineParser parser;
    parser.setApplicationDescription(
                "ld-chroma-decoder - Colourisation filter for ld-decode\n"
                "\n"
                "GPLv3 Open-Source - github: https://github.com/happycube/ld-decode");
    parser.addHelpOption();
    parser.addVersionOption();

    // -- General options --
    addStandardDebugOptions(parser);

    QCommandLineOption inputMetadataOption(QStringList() << "input-metadata",
                                       QCoreApplication::translate("main", "Specify the input metadata file (default input.db or input.json)"),
                                       QCoreApplication::translate("main", "filename"));
    parser.addOption(inputMetadataOption);

    QCommandLineOption inputJsonOption(QStringList() << "input-json",
                                       QCoreApplication::translate("main", "Specify the input metadata file (legacy alias for --input-metadata)"),
                                       QCoreApplication::translate("main", "filename"));
    parser.addOption(inputJsonOption);

    QCommandLineOption startFrameOption(QStringList() << "s" << "start",
                                        QCoreApplication::translate("main", "Specify the start frame number"),
                                        QCoreApplication::translate("main", "number"));
    parser.addOption(startFrameOption);

    QCommandLineOption lengthOption(QStringList() << "l" << "length",
                                        QCoreApplication::translate("main", "Specify the length (number of frames to process)"),
                                        QCoreApplication::translate("main", "number"));
    parser.addOption(lengthOption);

    QCommandLineOption setReverseOption(QStringList() << "r" << "reverse",
                                       QCoreApplication::translate("main", "Reverse the field order to second/first (default first/second)"));
    parser.addOption(setReverseOption);

    QCommandLineOption chromaGainOption(QStringList() << "chroma-gain",
                                        QCoreApplication::translate("main", "Gain factor applied to chroma components (default 1.0)"),
                                        QCoreApplication::translate("main", "number"));
    parser.addOption(chromaGainOption);

    QCommandLineOption chromaPhaseOption(QStringList() << "chroma-phase",
                                        QCoreApplication::translate("main", "Phase rotation applied to chroma components (degrees; default 0.0)"),
                                        QCoreApplication::translate("main", "number"));
    parser.addOption(chromaPhaseOption);

    QCommandLineOption outputFormatOption(QStringList() << "p" << "output-format",
                                       QCoreApplication::translate("main", "Output format (rgb, yuv, y4m; default rgb); RGB48, YUV444P16, GRAY16 pixel formats are supported"),
                                       QCoreApplication::translate("main", "output-format"));
    parser.addOption(outputFormatOption);

    QCommandLineOption setBwModeOption(QStringList() << "b" << "blackandwhite",
                                       QCoreApplication::translate("main", "Output in black and white"));
    parser.addOption(setBwModeOption);

    QCommandLineOption outputPaddingOption(QStringList() << "pad" << "output-padding",
                                       QCoreApplication::translate("main", "Pad the output frame to a multiple of this many pixels on both axes (1 means no padding, maximum is 32)"),
                                       QCoreApplication::translate("main", "number"));
    parser.addOption(outputPaddingOption);

    QCommandLineOption decoderOption(QStringList() << "f" << "decoder",
                                     QCoreApplication::translate("main", "Decoder to use (ntsc1d, ntsc2d, ntsc3d, ntsc3dnoadapt, mono; default automatic)"),
                                     QCoreApplication::translate("main", "decoder"));
    parser.addOption(decoderOption);

    QCommandLineOption threadsOption(QStringList() << "t" << "threads",
                                     QCoreApplication::translate("main", "Specify the number of concurrent threads (default number of logical CPUs)"),
                                     QCoreApplication::translate("main", "number"));
    parser.addOption(threadsOption);

    QCommandLineOption firstFieldLineOption(QStringList() << "ffll" << "first_active_field_line",
                                            QCoreApplication::translate("main", "The first visible line of a field. Range 1-259 for NTSC (default: 20)"),
                                            QCoreApplication::translate("main", "number"));
    parser.addOption(firstFieldLineOption);

    QCommandLineOption lastFieldLineOption(QStringList() << "lfll" << "last_active_field_line",
                                           QCoreApplication::translate("main", "The last visible line of a field. Range 1-259 for NTSC (default: 259)"),
                                           QCoreApplication::translate("main", "number"));
    parser.addOption(lastFieldLineOption);

    QCommandLineOption firstFrameLineOption(QStringList() << "ffrl" << "first_active_frame_line",
                                            QCoreApplication::translate("main", "The first visible line of a frame. Range 1-525 for NTSC (default: 40)"),
                                            QCoreApplication::translate("main", "number"));
    parser.addOption(firstFrameLineOption);

    QCommandLineOption lastFrameLineOption(QStringList() << "lfrl" << "last_active_frame_line",
                                           QCoreApplication::translate("main", "The last visible line of a frame. Range 1-525 for NTSC (default: 525)"),
                                           QCoreApplication::translate("main", "number"));
    parser.addOption(lastFrameLineOption);

    // -- NTSC decoder options --
    QCommandLineOption showMapOption(QStringList() << "o" << "oftest",
                                     QCoreApplication::translate("main", "NTSC: Overlay the adaptive filter map (only used for testing)"));
    parser.addOption(showMapOption);

    QCommandLineOption chromaNROption(QStringList() << "chroma-nr",
                                      QCoreApplication::translate("main", "NTSC: Chroma noise reduction level in dB (default 0.0)"),
                                      QCoreApplication::translate("main", "number"));
    parser.addOption(chromaNROption);

    QCommandLineOption lumaNROption(QStringList() << "luma-nr",
                                    QCoreApplication::translate("main", "Luma noise reduction level in dB (default 0.0)"),
                                    QCoreApplication::translate("main", "number"));
    parser.addOption(lumaNROption);

    QCommandLineOption ntscPhaseCompOption(QStringList() << "ntsc-phase-comp",
                                           QCoreApplication::translate("main", "NTSC: Adjust phase per-line using burst phase"));
    parser.addOption(ntscPhaseCompOption);

    QCommandLineOption adaptThresholdOption(QStringList() << "adapt-threshold",
                                            QCoreApplication::translate("main", "NTSC: 3D adaptive filter threshold (default 1.0, higher = more 3D)"),
                                            QCoreApplication::translate("main", "number"));
    parser.addOption(adaptThresholdOption);

    QCommandLineOption chromaWeightOption(QStringList() << "chroma-weight",
                                          QCoreApplication::translate("main", "NTSC: Chroma weight for 3D adaptive filter (default 1.0, higher = more 2D)"),
                                          QCoreApplication::translate("main", "number"));
    parser.addOption(chromaWeightOption);

    // -- Positional arguments --
    parser.addPositionalArgument("input", QCoreApplication::translate("main", "Specify input TBC file (- for piped input)"));
    parser.addPositionalArgument("output", QCoreApplication::translate("main", "Specify output file (omit or - for piped output)"));

    // Process the command line options
    parser.process(a);
    processStandardDebugOptions(parser);
    

    QString inputFileName;
    QString outputFileName = "-";
    QStringList positionalArguments = parser.positionalArguments();
    if (positionalArguments.count() == 2) {
        inputFileName = positionalArguments.at(0);
        outputFileName = positionalArguments.at(1);
    } else if (positionalArguments.count() == 1) {
        inputFileName = positionalArguments.at(0);
    } else {
        qCritical("You must specify the input TBC and output files");
        return -1;
    }

    if (inputFileName == "-" && !parser.isSet(inputMetadataOption)) {
        qCritical("With piped input, you must also specify the input metadata file");
        return -1;
    }
    if (inputFileName == outputFileName && outputFileName != "-") {
        qCritical("Input and output files cannot be the same");
        return -1;
    }

    qint32 startFrame = -1;
    qint32 length = -1;
    qint32 maxThreads = QThread::idealThreadCount();
    
    Comb::Configuration combConfig;
    MonoDecoder::MonoConfiguration monoConfig;
    OutputWriter::Configuration outputConfig;

    if (parser.isSet(startFrameOption)) {
        startFrame = parser.value(startFrameOption).toInt();
        if (startFrame < 1) {
            qCritical("Specified startFrame must be at least 1");
            return -1;
        }
    }

    if (parser.isSet(lengthOption)) {
        length = parser.value(lengthOption).toInt();
        if (length < 1) {
            qCritical("Specified length must be greater than zero frames");
            return -1;
        }
    }

    if (parser.isSet(threadsOption)) {
        maxThreads = parser.value(threadsOption).toInt();
        if (maxThreads < 1) {
            qCritical("Specified number of threads must be greater than zero");
            return -1;
        }
    }

    if (parser.isSet(chromaGainOption)) {
        const double value = parser.value(chromaGainOption).toDouble();
        combConfig.chromaGain = value;
        if (value < 0.0) {
            qCritical("Chroma gain must not be less than 0");
            return -1;
        }
    }

    if (parser.isSet(chromaPhaseOption)) {
        combConfig.chromaPhase = parser.value(chromaPhaseOption).toDouble();
    }

    bool bwMode = parser.isSet(setBwModeOption);
    if (bwMode) {
        combConfig.chromaGain = 0.0;
    }

    if (parser.isSet(showMapOption)) {
        combConfig.showMap = true;
    }

    if (parser.isSet(chromaNROption)) {
        combConfig.cNRLevel = parser.value(chromaNROption).toDouble();
        if (combConfig.cNRLevel < 0.0) {
            qCritical("Chroma noise reduction cannot be negative");
            return -1;
        }
    }

    if (parser.isSet(lumaNROption)) {
        combConfig.yNRLevel = parser.value(lumaNROption).toDouble();
        monoConfig.yNRLevel = parser.value(lumaNROption).toDouble();
        if (combConfig.yNRLevel < 0.0) {
            qCritical("Luma noise reduction cannot be negative");
            return -1;
        }
    }

    if (parser.isSet(ntscPhaseCompOption)) {
        combConfig.phaseCompensation = true;
    }

    if (parser.isSet(adaptThresholdOption)) {
        combConfig.adaptThreshold = parser.value(adaptThresholdOption).toDouble();
        if (combConfig.adaptThreshold <= 0.0) {
            qCritical("Adapt threshold must be greater than 0");
            return -1;
        }
    }

    if (parser.isSet(chromaWeightOption)) {
        combConfig.chromaWeight = parser.value(chromaWeightOption).toDouble();
        if (combConfig.chromaWeight < 0.0) {
            qCritical("Chroma weight must be greater than or equal to 0");
            return -1;
        }
    }

    LdDecodeMetaData::LineParameters lineParameters;
    if (parser.isSet(firstFieldLineOption)) {
        lineParameters.firstActiveFieldLine = parser.value(firstFieldLineOption).toInt();
    }
    if (parser.isSet(lastFieldLineOption)) {
        lineParameters.lastActiveFieldLine = parser.value(lastFieldLineOption).toInt();
    }
    if (parser.isSet(firstFrameLineOption)) {
        lineParameters.firstActiveFrameLine = parser.value(firstFrameLineOption).toInt();
    }
    if (parser.isSet(lastFrameLineOption)) {
        lineParameters.lastActiveFrameLine = parser.value(lastFrameLineOption).toInt();
    }

    QString inputMetadataFileName = inputFileName + ".db";
    if (parser.isSet(inputMetadataOption)) {
        inputMetadataFileName = parser.value(inputMetadataOption);
    }

    LdDecodeMetaData metaData;
    if (!metaData.read(inputMetadataFileName)) {
        qCritical() << "Unable to open ld-decode metadata file:" << inputMetadataFileName;
        return -1;
    }
    
    metaData.processLineParameters(lineParameters);
    
    if (parser.isSet(setReverseOption)) {
        qInfo() << "Expected field order is reversed to second field/first field";
        metaData.setIsFirstFieldFirst(false);
    }

    // Default to NTSC 2D 
    QString decoderName = "ntsc2d";
    if (parser.isSet(decoderOption)) {
        decoderName = parser.value(decoderOption);
    }

    if (combConfig.showMap && decoderName != "ntsc3d") {
        qCritical() << "Can only show adaptive filter map with the ntsc3d decoder";
        return -1;
    }

    std::unique_ptr<Decoder> decoder;
    if (decoderName == "ntsc1d") {
        combConfig.dimensions = 1;
        decoder = std::make_unique<NtscDecoder>(combConfig);
    } else if (decoderName == "ntsc2d") {
        combConfig.dimensions = 2;
        decoder = std::make_unique<NtscDecoder>(combConfig);
    } else if (decoderName == "ntsc3d") {
        combConfig.dimensions = 3;
        decoder = std::make_unique<NtscDecoder>(combConfig);
    } else if (decoderName == "ntsc3dnoadapt") {
        combConfig.dimensions = 3;
        combConfig.adaptive = false;
        decoder = std::make_unique<NtscDecoder>(combConfig);
    } else if (decoderName == "mono") {
        decoder = std::make_unique<MonoDecoder>(monoConfig);
    } else {
        qCritical() << "Unknown decoder" << decoderName;
        return -1;
    }

    QString outputFormatName;
    if (parser.isSet(outputFormatOption)) {
        outputFormatName = parser.value(outputFormatOption);
    } else {
        outputFormatName = "rgb";
    }
    
    if (outputFormatName == "yuv" || outputFormatName == "y4m") {
        if (outputFormatName == "y4m") {
            outputConfig.outputY4m = true;
        }
        if (bwMode || decoderName == "mono") {
            outputConfig.pixelFormat = OutputWriter::PixelFormat::GRAY16;
        } else {
            outputConfig.pixelFormat = OutputWriter::PixelFormat::YUV444P16;
        }
    } else if (outputFormatName == "rgb") {
        outputConfig.pixelFormat = OutputWriter::PixelFormat::RGB48;
    } else {
        qCritical() << "Unknown output format" << outputFormatName;
        return -1;
    }

    if (parser.isSet(outputPaddingOption)) {
        outputConfig.paddingAmount = parser.value(outputPaddingOption).toInt();
        if (outputConfig.paddingAmount < 1 || outputConfig.paddingAmount > 32) {
            qInfo() << "Invalid value" << outputConfig.paddingAmount << "specified for padding amount, defaulting to 8.";
            outputConfig.paddingAmount = 8;
        }
    }
    
    DecoderPool decoderPool(*decoder, inputFileName, metaData, outputConfig, outputFileName, startFrame, length, maxThreads);
    if (!decoderPool.process()) {
        return -1;
    }

    return 0;
}