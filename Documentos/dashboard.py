import tkinter as tk
from tkinter import ttk
import paho.mqtt.client as mqtt
import json
import datetime

# --- CONFIGURACIÓN MQTT ---
BROKER = "broker.emqx.io"
PORT = 1883
TOPIC_SUB_DATOS = "udb/grupo5/fase2/sensores"
TOPIC_SUB_EVENTOS = "udb/grupo5/fase2/eventos"
TOPIC_PUB_CONTROL = "udb/grupo5/fase2/control"

def on_connect(client, userdata, flags, rc):
    print(f"Conectado al Broker MQTT (Código: {rc})")
    client.subscribe([(TOPIC_SUB_DATOS, 0), (TOPIC_SUB_EVENTOS, 0)])
    var_estado_mqtt.set("Conectado a MQTT")
    lbl_estado.config(foreground="green")

def on_message(client, userdata, msg):
    try:
        tema = msg.topic
        payload = msg.payload.decode("utf-8")
        
        if tema == TOPIC_SUB_DATOS:
            datos = json.loads(payload)
            
            # Actualizar Telemetría
            var_temp.set(f"{datos.get('temp', 0)} °C")
            var_hum.set(f"{datos.get('hum', 0)} %")
            var_gas.set(f"{datos.get('gas', 0)}")
            var_umbral.set(f"{datos.get('umbralGas', 0)}")
            var_luz.set(f"{datos.get('luz', 0)}")
            
            estado_mov = "¡Detectado!" if datos.get('mov', 0) == 1 else "Despejado"
            var_mov.set(estado_mov)
            
            # Actualizar Estado de Actuadores
            var_fan.set("ENCENDIDO" if datos.get('ventilador', 0) == 1 else "APAGADO")
            var_led.set("ENCENDIDA" if datos.get('ledLuz', 0) == 1 else "APAGADA")
            var_door.set("ABIERTA" if datos.get('puerta', 0) == 1 else "CERRADA")
            
            # Manejo de Alerta de Gas
            if datos.get('alertaGas', 0) == 1:
                var_alerta.set("¡PELIGRO! ALERTA DE GAS")
                lbl_alerta.config(foreground="red", font=("Arial", 11, "bold"))
            else:
                var_alerta.set("SISTEMA SEGURO")
                lbl_alerta.config(foreground="green", font=("Arial", 11, "bold"))
                
        elif tema == TOPIC_SUB_EVENTOS:
            # Registrar el evento en el log con hora actual
            hora_actual = datetime.datetime.now().strftime("%H:%M:%S")
            evento_formateado = f"[{hora_actual}] {payload}\n"
            
            # Insertar en el Text widget (requiere habilitar/deshabilitar escritura temporalmente)
            txt_eventos.config(state="normal")
            txt_eventos.insert(tk.END, evento_formateado)
            txt_eventos.see(tk.END) # Auto-scroll hacia abajo
            txt_eventos.config(state="disabled")
            
    except Exception as e:
        print("Error procesando mensaje:", e)

# --- FUNCIONES DE CONTROL ---
def enviar_comando(comando):
    client.publish(TOPIC_PUB_CONTROL, comando)
    print(f"Comando enviado: {comando}")

def limpiar_log():
    txt_eventos.config(state="normal")
    txt_eventos.delete(1.0, tk.END)
    txt_eventos.config(state="disabled")

# --- INTERFAZ GRÁFICA (TKINTER) ---
root = tk.Tk()
root.title("Dashboard IoT - Fase 2 (Grupo 5)")
root.geometry("600x750")
root.config(padx=15, pady=15)

# Estilos
style = ttk.Style()
style.configure("TLabel", font=("Arial", 10))
style.configure("TButton", font=("Arial", 10))
style.configure("TLabelframe.Label", font=("Arial", 11, "bold"))

# Variables dinámicas
var_estado_mqtt = tk.StringVar(value="Conectando...")
var_temp = tk.StringVar(value="-- °C")
var_hum = tk.StringVar(value="-- %")
var_gas = tk.StringVar(value="--")
var_umbral = tk.StringVar(value="--")
var_luz = tk.StringVar(value="--")
var_mov = tk.StringVar(value="--")

var_fan = tk.StringVar(value="--")
var_led = tk.StringVar(value="--")
var_door = tk.StringVar(value="--")
var_alerta = tk.StringVar(value="--")

# --- HEADER ---
lbl_estado = ttk.Label(root, textvariable=var_estado_mqtt, font=("Arial", 10, "italic"))
lbl_estado.pack(pady=(0, 10))

# --- SECCIÓN 1: TELEMETRÍA ---
frame_sensores = ttk.LabelFrame(root, text=" Sensores y Telemetría ", padding=(10, 10))
frame_sensores.pack(fill="x", pady=5)

# Layout de sensores en Grid (2 columnas visuales)
ttk.Label(frame_sensores, text="Temperatura:").grid(row=0, column=0, sticky="w", pady=2)
ttk.Label(frame_sensores, textvariable=var_temp, font=("Arial", 10, "bold")).grid(row=0, column=1, sticky="e", padx=20)

ttk.Label(frame_sensores, text="Humedad:").grid(row=1, column=0, sticky="w", pady=2)
ttk.Label(frame_sensores, textvariable=var_hum, font=("Arial", 10, "bold")).grid(row=1, column=1, sticky="e", padx=20)

ttk.Label(frame_sensores, text="Movimiento (PIR):").grid(row=2, column=0, sticky="w", pady=2)
ttk.Label(frame_sensores, textvariable=var_mov, font=("Arial", 10, "bold")).grid(row=2, column=1, sticky="e", padx=20)

ttk.Label(frame_sensores, text="Nivel de Luz (LDR):").grid(row=0, column=2, sticky="w", pady=2, padx=(30,0))
ttk.Label(frame_sensores, textvariable=var_luz, font=("Arial", 10, "bold")).grid(row=0, column=3, sticky="e")

ttk.Label(frame_sensores, text="Nivel de Gas:").grid(row=1, column=2, sticky="w", pady=2, padx=(30,0))
ttk.Label(frame_sensores, textvariable=var_gas, font=("Arial", 10, "bold")).grid(row=1, column=3, sticky="e")

ttk.Label(frame_sensores, text="Umbral de Gas:").grid(row=2, column=2, sticky="w", pady=2, padx=(30,0))
ttk.Label(frame_sensores, textvariable=var_umbral, font=("Arial", 10, "bold")).grid(row=2, column=3, sticky="e")

# --- SECCIÓN 2: ESTADO DEL SISTEMA ---
frame_estado = ttk.LabelFrame(root, text=" Estado de Actuadores ", padding=(10, 10))
frame_estado.pack(fill="x", pady=10)

ttk.Label(frame_estado, text="Ventilador (Relé):").grid(row=0, column=0, sticky="w", pady=2)
ttk.Label(frame_estado, textvariable=var_fan, font=("Arial", 10, "bold")).grid(row=0, column=1, sticky="e", padx=20)

ttk.Label(frame_estado, text="Luz Principal (LED):").grid(row=1, column=0, sticky="w", pady=2)
ttk.Label(frame_estado, textvariable=var_led, font=("Arial", 10, "bold")).grid(row=1, column=1, sticky="e", padx=20)

ttk.Label(frame_estado, text="Puerta (Servo):").grid(row=0, column=2, sticky="w", pady=2, padx=(30,0))
ttk.Label(frame_estado, textvariable=var_door, font=("Arial", 10, "bold")).grid(row=0, column=3, sticky="e")

ttk.Label(frame_estado, text="Estado de Seguridad:").grid(row=1, column=2, sticky="w", pady=2, padx=(30,0))
lbl_alerta = tk.Label(frame_estado, textvariable=var_alerta) # Usamos tk.Label para controlar mejor el color
lbl_alerta.grid(row=1, column=3, sticky="e")

# --- SECCIÓN 3: PANEL DE CONTROL MANUAL ---
frame_control = ttk.LabelFrame(root, text=" Panel de Control Remoto ", padding=(10, 10))
frame_control.pack(fill="x", pady=5)

# Fila 1: Ventilador
tk.Button(frame_control, text="Encender Ventilador", bg="#4CAF50", fg="white", command=lambda: enviar_comando("RELAY_ON")).grid(row=0, column=0, padx=5, pady=5, sticky="we")
tk.Button(frame_control, text="Apagar Ventilador", bg="#F44336", fg="white", command=lambda: enviar_comando("RELAY_OFF")).grid(row=0, column=1, padx=5, pady=5, sticky="we")

# Fila 2: Luz
tk.Button(frame_control, text="Encender Luz", bg="#FFC107", fg="black", command=lambda: enviar_comando("LED_ON")).grid(row=1, column=0, padx=5, pady=5, sticky="we")
tk.Button(frame_control, text="Apagar Luz", bg="#607D8B", fg="white", command=lambda: enviar_comando("LED_OFF")).grid(row=1, column=1, padx=5, pady=5, sticky="we")

# Fila 3: Puerta
tk.Button(frame_control, text="Abrir Puerta", bg="#2196F3", fg="white", command=lambda: enviar_comando("SERVO_OPEN")).grid(row=2, column=0, padx=5, pady=5, sticky="we")
tk.Button(frame_control, text="Cerrar Puerta", bg="#FF9800", fg="white", command=lambda: enviar_comando("SERVO_CLOSE")).grid(row=2, column=1, padx=5, pady=5, sticky="we")

# Fila 4: Reset Fail-Safe
tk.Button(frame_control, text="⚠️ RESETEAR SISTEMA DE GAS ⚠️", bg="#B71C1C", fg="white", font=("Arial", 10, "bold"), command=lambda: enviar_comando("RESET")).grid(row=3, column=0, columnspan=2, padx=5, pady=(10, 5), sticky="we")

frame_control.columnconfigure(0, weight=1)
frame_control.columnconfigure(1, weight=1)

# --- SECCIÓN 4: LOG DE EVENTOS ---
frame_eventos = ttk.LabelFrame(root, text=" Registro de Eventos del Sistema ", padding=(10, 10))
frame_eventos.pack(fill="both", expand=True, pady=10)

txt_eventos = tk.Text(frame_eventos, height=8, state="disabled", bg="#f4f4f4", font=("Consolas", 9))
txt_eventos.pack(side=tk.LEFT, fill="both", expand=True)

# Scrollbar para el log de eventos
scrollbar = ttk.Scrollbar(frame_eventos, command=txt_eventos.yview)
scrollbar.pack(side=tk.RIGHT, fill="y")
txt_eventos.config(yscrollcommand=scrollbar.set)

ttk.Button(root, text="Limpiar Registro", command=limpiar_log).pack(pady=(0,5))

# --- INICIAR CLIENTE MQTT ---
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

try:
    client.connect(BROKER, PORT, 60)
    client.loop_start() 
except Exception as e:
    print("Error de conexión:", e)
    var_estado_mqtt.set("Error de conexión")
    lbl_estado.config(foreground="red")

# Manejo de cierre de ventana limpio
def on_closing():
    client.loop_stop()
    client.disconnect()
    root.destroy()

root.protocol("WM_DELETE_WINDOW", on_closing)

# Iniciar la ventana gráfica
root.mainloop()