import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import paho.mqtt.client as mqtt
from datetime import datetime

# Datos
temp_data, humi_data, fecha_data = [], [], []
latest_values = {
    "temperatura": "N/A", "humedad": "N/A", "movimiento": "N/A",
    "distancia": "N/A", "fecha": "N/A", "servo": "N/A"
}
app_activa = True
MAX_PUNTOS = 100

# MQTT Callbacks
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Conectado a MQTT")
        client.subscribe("sensor/#")
    else:
        print("❌ Error de conexión MQTT:", rc)

def on_message(client, userdata, msg):
    if not app_activa: return
    topic = msg.topic
    payload = msg.payload.decode()
    print(f"[RECEBIDO] {topic} => {payload}")

    if topic == "sensor/temperatura":
        latest_values["temperatura"] = payload
        try: temp_data.append(float(payload))
        except: pass
    elif topic == "sensor/humedad":
        latest_values["humedad"] = payload
        try: humi_data.append(float(payload))
        except: pass
    elif topic == "sensor/movimiento":
        latest_values["movimiento"] = "Sí" if payload == "1" else "No"
    elif topic == "sensor/distancia":
        latest_values["distancia"] = payload
    elif topic == "sensor/fecha":
        latest_values["fecha"] = payload
        fecha_data.append(datetime.now().strftime("%H:%M:%S"))
    elif topic == "sensor/servo":
        latest_values["servo"] = payload
        actualizar_estado_servo(payload.lower())

    limitar_datos()

def limitar_datos():
    if len(temp_data) > MAX_PUNTOS: temp_data.pop(0)
    if len(humi_data) > MAX_PUNTOS: humi_data.pop(0)
    if len(fecha_data) > MAX_PUNTOS: fecha_data.pop(0)

def enviar_comando_servo(cmd):
    client.publish("home9/ebb2025/servo1", cmd)

def actualizar_ui():
    try:
        lbl_temp.config(text=f"Temperatura: {latest_values['temperatura']} °C")
        lbl_humi.config(text=f"Humedad: {latest_values['humedad']} %")
        lbl_mov.config(text=f"Movimiento: {latest_values['movimiento']}")
        lbl_dist.config(text=f"Distancia: {latest_values['distancia']} cm")
        lbl_fecha.config(text=f"Fecha: {latest_values['fecha']}")
        lbl_servo.config(text=f"Servo: {latest_values['servo']}")
        actualizar_grafico()
    except tk.TclError:
        pass

def actualizar_estado_servo(estado):
    if estado == "abierto":
        btn_abrir.configure(style="Activado.TButton")
        btn_cerrar.configure(style="Normal.TButton")
    elif estado == "cerrado":
        btn_cerrar.configure(style="Desactivado.TButton")
        btn_abrir.configure(style="Normal.TButton")

def actualizar_grafico():
    ax.clear()
    if fecha_data and temp_data:
        ax.plot(fecha_data[-20:], temp_data[-20:], label='Temp (°C)', color='red')
    if fecha_data and humi_data:
        ax.plot(fecha_data[-20:], humi_data[-20:], label='Humedad (%)', color='blue')
    ax.set_title("Datos en Tiempo Real")
    ax.set_xlabel("Hora")
    ax.set_ylabel("Valor")
    ax.legend(loc="upper left")
    fig.autofmt_xdate()
    canvas.draw()

def refrescar_periodicamente():
    actualizar_ui()
    root.after(1000, refrescar_periodicamente)

def on_closing():
    global app_activa
    app_activa = False
    client.loop_stop()
    client.disconnect()
    root.destroy()

# Tkinter UI
root = tk.Tk()
root.title("Dashboard ESP32 - MQTT")
root.protocol("WM_DELETE_WINDOW", on_closing)

frame = ttk.Frame(root, padding=10)
frame.grid(row=0, column=0)

lbl_temp = ttk.Label(frame, text="Temperatura: N/A"); lbl_temp.grid(row=0, column=0, sticky="w")
lbl_humi = ttk.Label(frame, text="Humedad: N/A"); lbl_humi.grid(row=1, column=0, sticky="w")
lbl_mov = ttk.Label(frame, text="Movimiento: N/A"); lbl_mov.grid(row=2, column=0, sticky="w")
lbl_dist = ttk.Label(frame, text="Distancia: N/A"); lbl_dist.grid(row=3, column=0, sticky="w")
lbl_fecha = ttk.Label(frame, text="Fecha: N/A"); lbl_fecha.grid(row=4, column=0, sticky="w")
lbl_servo = ttk.Label(frame, text="Servo: N/A"); lbl_servo.grid(row=5, column=0, sticky="w")

btn_frame = ttk.Frame(frame, padding=10)
btn_frame.grid(row=6, column=0, pady=10)
btn_abrir = ttk.Button(btn_frame, text="ABRIR SERVO", command=lambda: enviar_comando_servo("abrir"), style="Normal.TButton")
btn_abrir.grid(row=1, column=0, padx=5)
btn_cerrar = ttk.Button(btn_frame, text="CERRAR SERVO", command=lambda: enviar_comando_servo("cerrar"), style="Normal.TButton")
btn_cerrar.grid(row=1, column=1, padx=5)

# Gráfica
fig, ax = plt.subplots(figsize=(6, 3))
canvas = FigureCanvasTkAgg(fig, master=root)
canvas.get_tk_widget().grid(row=0, column=1)

# Estilos
style = ttk.Style()
style.configure("Activado.TButton", background="green", foreground="white")
style.configure("Desactivado.TButton", background="red", foreground="white")
style.configure("Normal.TButton", background="SystemButtonFace")

# MQTT Cliente
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("broker.hivemq.com", 1883, 60)
client.loop_start()

# Iniciar UI
refrescar_periodicamente()
root.mainloop()