from flask import Flask, render_template
import websocket
import json
import threading
import time

app = Flask(__name__)

esp32_data = {"temperature": 0, "humidity": 0, "ledState": "OFF", "floodCount": 0}
message_buffer = ""

def on_message(ws, message):
    global esp32_data, message_buffer
    message_buffer += message
    print(f"Raw message received (buffered): {message_buffer}")
    try:
        data = json.loads(message_buffer)
        esp32_data.update(data)
        print(f"Updated esp32_data: {esp32_data}")
        message_buffer = ""
    except json.JSONDecodeError as e:
        print(f"JSON decode error: {e} (waiting for more data)")

def on_error(ws, error):
    print(f"WebSocket error: {error}")

def on_close(ws, close_status_code, close_msg):
    print(f"WebSocket closed: {close_status_code}, {close_msg}")
    print("Attempting to reconnect in 5 seconds...")
    time.sleep(5)
    start_websocket()

def on_open(ws):
    print("Connected to ESP32 WebSocket server")

def start_websocket():
    ws_url = "ws://myesp32ps.ddns.net:8081"  # Use your No-IP hostname
    ws = websocket.WebSocketApp(ws_url,
                                on_open=on_open,
                                on_message=on_message,
                                on_error=on_error,
                                on_close=on_close)
    ws.run_forever()

websocket_thread = threading.Thread(target=start_websocket)
websocket_thread.daemon = True
websocket_thread.start()
time.sleep(2)

@app.route('/')
def index():
    print(f"Serving data: {esp32_data}")
    return render_template('index.html', data=esp32_data)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)