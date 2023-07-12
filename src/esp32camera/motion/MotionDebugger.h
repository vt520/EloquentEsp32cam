//
// Created by Simone on 05/07/23.
//

#ifndef ELOQUENTESP32CAM_MOTIONDEBUGGER_H
#define ELOQUENTESP32CAM_MOTIONDEBUGGER_H

#ifndef MOTION_DEBUGGER_STACK_SIZE
#define MOTION_DEBUGGER_STACK_SIZE 6000
#endif

#ifndef MOTION_DEBUGGER_MOTION_STACK_SIZE
#define MOTION_DEBUGGER_MOTION_STACK_SIZE 15000
#endif

#ifndef MOTION_DEBUGGER_WEBSOCKET_STACK_SIZE
#define MOTION_DEBUGGER_WEBSOCKET_STACK_SIZE 10000
#endif

#include <WebSocketsServer.h>
#include "./MotionDetection.h"
#include "../../traits/IsHttpServer.h"
#include "../../extra/Thread.h"
#include "../http/MjpegStream.h"
#include "../../extra/ascii.h"
#include "../../extra/JsonBuilder.h"
#include "../js/MotionDebugger.js.h"


namespace Eloquent {
    namespace Esp32cam {
        namespace Motion {

            /**
             *
             */
            class MotionDebugger : public Trait::IsHttpServer {
            public:
                WebSocketsServer webSocket;
                bool isWsConnected;

                /**
                 *
                 */
                MotionDebugger() :
                        webSocket(82),
                        isWsConnected(false),
                        _mainThread("MotionDebuggerHttp", MOTION_DEBUGGER_STACK_SIZE),
                        _motionThread("Motion", MOTION_DEBUGGER_MOTION_STACK_SIZE),
                        _webSocketThread("MotionDebuggerWebSocket", MOTION_DEBUGGER_WEBSOCKET_STACK_SIZE) {

                }

                /**
                 *
                 * @return
                 */
                bool begin() {
                    // start MJPEG stream
                    if (!mjpegStream.begin(81))
                        return setErrorMessage(mjpegStream.getErrorMessage());

                    // run motion detection in his own thread
                    _motionThread
                        .withArgs((void*) this)
                        .run([](void *args) {
                            MotionDebugger *self = (MotionDebugger*) args;

                            while (!self->isWsConnected)
                                delay(1);

                            ESP_LOGI("MotionDebugger", "Motion detection STARTED");

                            while (true) {
                                motionDetection.update();
                                delay(10);
                                yield();
                            }
                        });

                    // index page
                    on("/", [this]() {
                        html.open([this]() {
                            html.svelteApp();
                        });
                    });

                    on("/app.js", [this]() {
                        httpServer.server.send(200, "application/javascript", JS_APP);
                    });

                    setupWebsocket();

                    return startServer(_mainThread);
                }

            protected:
                Eloquent::Extra::Thread _mainThread;
                Eloquent::Extra::Thread _motionThread;
                Eloquent::Extra::Thread _webSocketThread;

                /**
                 * Get server name
                 * @return
                 */
                const char * getServerName() override {
                    return "MotionDebugger";
                }

                /**
                 * Display more info
                 */
                String getMoreWelcomeMessage() override {
                    return 
                        String(F(" - :81/mjpeg -> MJPEG stream\n"))
                        + String(F(" - :82 -> WebSocket server"))
                    ;
                }

                /**
                 * Listen for WebSocket connections
                 */
                void setupWebsocket() {
                    webSocket.begin();
                    webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
                        // on websocket connection, continously stream motion background + mask
                        if (type == WStype_CONNECTED) {
                            isWsConnected = true;
                            delay(2000);

                            while (true) {
                                if (motionDetection.isOk()) {
                                    const uint16_t width = jpeg.getDecodedWidth();
                                    const uint16_t height = jpeg.getDecodedHeight();
                                    
                                    Eloquent::Extra::JsonBuilder json(100);

                                    json.object();
                                    json.add("width", width);
                                    json.add("height", height);
                                    json.add("detected", motionDetection.detected());
                                    json.add("benchmark", motionDetection.benchmark.inMillis());
                                    json.add("mask", motionDetection.foregroundMask.array.toASCII());
                                    json.prop("background");
                                    json.raw('"');

                                    webSocket.sendTXT(num, json.toString());
                                    sendBackground(num, width, height);
                                    webSocket.sendTXT(num, "\"}\n");
                                }
                            }
                        }
                    });

                    _webSocketThread
                        .withArgs((void*) &webSocket)
                        .run([](void *args) {
                            WebSocketsServer *webSocket = (WebSocketsServer*) args;

                            while (true) {
                                webSocket->loop();
                                yield();
                            }
                        });
                }

                /**
                 * 
                 */
                void sendBackground(uint8_t num, uint16_t width, uint16_t height) {
                    String row;
                    size_t i = 0;

                    row.reserve(width + 1);

                    for (size_t y = 0; y < height; y++) {
                        row = "";

                        for (uint16_t x = 0; x < width; x++) {
                            const uint8_t pixel = motionDetection.background[i];
                            const uint8_t pos = (pixel >> 2) & 0x3F; 

                            row += SIMPLIFIED_ASCII_ALPHABET[pos];
                            i += 1;
                        }

                        webSocket.sendTXT(num, row);
                    }
                }
            };
        }
    }
}


static Eloquent::Esp32cam::Motion::MotionDebugger motionDebugger;

#endif //ELOQUENTESP32CAM_MOTIONDEBUGGER_H
