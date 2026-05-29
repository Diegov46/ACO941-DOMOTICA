import tkinter as tk
from tkinter import ttk
import paho.mqtt.client as mqtt
import json

# --- CONFIGURACIÓN MQTT ---
BROKER = "broker.emqx.io"
PORT = 1883
TOPIC_SUB = "udb/grupo5/fase1/sensores"
TOPIC_PUB = "udb/grupo5/fase1/control"

def on_connect(client, userdata, flags, rc):
    print(f"Conectado al Broker con código de resultado: {rc}")
    client.subscribe(TOPIC_SUB)
    lbl_estado.config(text="Estado: Conectado a MQTT", fg="green")

def on_message(client, userdata, msg):
    try:
        # Decodificar el JSON recibido desde el ESP32
        payload = json.loads(msg.payload.decode("utf-8"))
        
        # Actualizar las etiquetas en la interfaz
        var_temp.set(f"{payload.get('temp', 0)} °C")
        var_hum.set(f"{payload.get('hum', 0)} %")
        var_gas.set(f"{payload.get('gas', 0)}")
        var_luz.set(f"{payload.get('luz', 0)}")
        
        estado_mov = "¡Detectado!" if payload.get('mov', 0) == 1 else "Despejado"
        var_mov.set(estado_mov)
        
    except Exception as e:
        print("Error decodificando JSON:", e)

# --- FUNCIONES DE CONTROL (BOTONES) ---
def enviar_comando(comando):
    client.publish(TOPIC_PUB, comando)
    print(f"Comando enviado: {comando}")

# --- INTERFAZ GRÁFICA (TKINTER) ---
root = tk.Tk()
root.title("Dashboard IoT - Control del Hogar")
root.geometry("400x450")
root.config(padx=20, pady=20)

# Variables dinámicas para la interfaz
var_temp = tk.StringVar(value="-- °C")
var_hum = tk.StringVar(value="-- %")
var_gas = tk.StringVar(value="--")
var_mov = tk.StringVar(value="--")
var_luz = tk.StringVar(value="--")

# Sección: Estado de conexión
lbl_estado = tk.Label(root, text="Estado: Conectando...", font=("Arial", 10, "italic"))
lbl_estado.pack(pady=5)

# Sección: Visualización de Sensores
frame_sensores = tk.LabelFrame(root, text=" Telemetría de Sensores ", font=("Arial", 12, "bold"), padx=10, pady=10)
frame_sensores.pack(fill="x", pady=10)

tk.Label(frame_sensores, text="Temperatura:").grid(row=0, column=0, sticky="w", pady=2)
tk.Label(frame_sensores, textvariable=var_temp, font=("Arial", 10, "bold")).grid(row=0, column=1, sticky="e")

tk.Label(frame_sensores, text="Humedad:").grid(row=1, column=0, sticky="w", pady=2)
tk.Label(frame_sensores, textvariable=var_hum, font=("Arial", 10, "bold")).grid(row=1, column=1, sticky="e")

tk.Label(frame_sensores, text="Nivel de Gas:").grid(row=2, column=0, sticky="w", pady=2)
tk.Label(frame_sensores, textvariable=var_gas, font=("Arial", 10, "bold")).grid(row=2, column=1, sticky="e")

tk.Label(frame_sensores, text="Movimiento:").grid(row=3, column=0, sticky="w", pady=2)
tk.Label(frame_sensores, textvariable=var_mov, font=("Arial", 10, "bold")).grid(row=3, column=1, sticky="e")

tk.Label(frame_sensores, text="Nivel de Luz:").grid(row=4, column=0, sticky="w", pady=2)
tk.Label(frame_sensores, textvariable=var_luz, font=("Arial", 10, "bold")).grid(row=4, column=1, sticky="e")

# Sección: Control Remoto (Actuadores)
frame_control = tk.LabelFrame(root, text=" Panel de Control ", font=("Arial", 12, "bold"), padx=10, pady=10)
frame_control.pack(fill="x", pady=10)

tk.Button(frame_control, text="Encender Relé", bg="#4CAF50", fg="white", command=lambda: enviar_comando("RELAY_ON")).grid(row=0, column=0, padx=5, pady=5, sticky="we")
tk.Button(frame_control, text="Apagar Relé", bg="#F44336", fg="white", command=lambda: enviar_comando("RELAY_OFF")).grid(row=0, column=1, padx=5, pady=5, sticky="we")

tk.Button(frame_control, text="Abrir Puerta", bg="#2196F3", fg="white", command=lambda: enviar_comando("SERVO_OPEN")).grid(row=1, column=0, padx=5, pady=5, sticky="we")
tk.Button(frame_control, text="Cerrar Puerta", bg="#FF9800", fg="white", command=lambda: enviar_comando("SERVO_CLOSE")).grid(row=1, column=1, padx=5, pady=5, sticky="we")

frame_control.columnconfigure(0, weight=1)
frame_control.columnconfigure(1, weight=1)

# --- INICIAR CLIENTE MQTT ---
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

try:
    client.connect(BROKER, PORT, 60)
    client.loop_start() # Corre MQTT en un hilo en segundo plano
except Exception as e:
    print("Error de conexión:", e)
    lbl_estado.config(text="Estado: Error de conexión", fg="red")

# Iniciar la ventana gráfica
root.mainloop()

# Detener MQTT al cerrar la ventana
client.loop_stop()
client.disconnect()