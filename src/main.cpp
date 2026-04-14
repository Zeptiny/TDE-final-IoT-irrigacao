#define BLYNK_TEMPLATE_ID   "TMPL2WODQMGv3"
#define BLYNK_TEMPLATE_NAME "TRABALHO FINAL IOT"
#define BLYNK_AUTH_TOKEN    "nlhG7jxJfG5u9qFkqS6YgAwSbYnfMIdr"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>

// MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Credenciais da rede
char ssid[] = "Vida Nova"; 
char pass[] = "fevereiro24"; 

// Objetos
Servo meuServo;
BlynkTimer timer;

// Pinos
const int pinoSensor = 34; 
const int pinoServo  = 18; 
int estadoAnteriorServo = -1;

// Variáveis de Controle
bool modoManualForcado = false; // Status do botão V2
int umidadePercentual = 0;      // Leitura atual do sensor
int limiteUmidade = 0;          // Meta de umidade escolhida no Slider (V3)

//Funçaõ par analisar se o problema é o servo ou a peca atras dele
void moverServo(int anguloAlvo) {
  static int anguloAtual = 10; // Começa no 10
  meuServo.attach(pinoServo, 500, 2400);
  delay(50);

  // Move de 1 em 1 grau para não dar pico de corrente
  if (anguloAlvo > anguloAtual) {
    for (int pos = anguloAtual; pos <= anguloAlvo; pos++) {
      meuServo.write(pos);
      delay(15); // Velocidade do movimento
    }
  } else {
    for (int pos = anguloAtual; pos >= anguloAlvo; pos--) {
      meuServo.write(pos);
      delay(15);
    }
  }

  anguloAtual = anguloAlvo; // Atualiza a posição atual
  delay(200);
  meuServo.detach(); 
  Serial.println("Movimento suave concluído.");
}

//LÓGICA DO SLIDER (V3): Define a porcentagem de umidade desejada 
BLYNK_WRITE(V3) {
  int valorSlider = param.asInt();

  // TRAVA: Se o manual estiver ligado, o slider é ignorado e volta para 0
  if (modoManualForcado == true && valorSlider > 0) {
    Blynk.virtualWrite(V3, 0); 
    Serial.println("BLOQUEIO: Desligue o modo manual para ajustar a meta de umidade.");
    return;
  }

  // Define a nova meta de umidade para o solo
  limiteUmidade = valorSlider;
  
  // Mensagem solicitada
  Serial.print("O valor da umidade escolhio está em ");
  Serial.print(limiteUmidade);
  Serial.println(" por cento.");
}

// LÓGICA DO BOTÃO MANUAL (V2): Liga/Desliga forçado 
BLYNK_WRITE(V2) {
  int estadoBotao = param.asInt();
  
  // TRAVA: Se você já definiu uma meta no Slider, o botão manual não pode ser ligado
  if (limiteUmidade > 0 && estadoBotao == 1) {
    Blynk.virtualWrite(V2, 0); 
    Serial.println("BLOQUEIO: Zere o Slider (0%) para assumir o controle manual.");
    modoManualForcado = false;
  } 
  else {
    modoManualForcado = (estadoBotao == 1);
    
    if (modoManualForcado) {
      moverServo(80); // Abre totalmente a bomba no manual
      Serial.println("MODO MANUAL ATIVADO: Bomba ligada via App.");
    } else {
      moverServo(10);  // Fecha a bomba
      Serial.println("MODO MANUAL DESATIVADO: Bomba desligada.");
    }
  }
}

// PROCESSAMENTO: Leitura e Decisão Automática
void processarDadosSistema() {
  int valorBruto = analogRead(pinoSensor);
  umidadePercentual = map(valorBruto, 4095, 0, 0, 100); 
  Serial.println("Umidade Atual: " + String(umidadePercentual) + "%");
  
  if (mqttClient.connected()) {
    mqttClient.publish("ds/umidade", String(umidadePercentual).c_str());
  }

  // Se o manual estiver ligado, a automação para aqui
  if (modoManualForcado == true) return; 

  // Modo Automático: Só age se o Slider for maior que 0
  if (limiteUmidade > 0) {
    if (umidadePercentual < limiteUmidade) {
      // SÓ MOVE SE ESTIVER FECHADO
      if (estadoAnteriorServo != 1) { 
        Serial.println("AUTOMÁTICO: Solo seco. Abrindo...");
        moverServo(80); 
        estadoAnteriorServo = 1; // Marca como Aberto
      }
    } else {
      // SÓ MOVE SE ESTIVER ABERTO
      if (estadoAnteriorServo != 0) {
        Serial.println("AUTOMÁTICO: Meta atingida. Fechando...");
        moverServo(10); 
        estadoAnteriorServo = 0; // Marca como Fechado
      }
    }
  }
}

//
// MQTT
//

// OBS: Por enquanto não está inscrito em nenhum tópico
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}


void mqttReconnect() {
  if (mqttClient.connected()) return;

  Serial.print("MQTT: Tentando reconectar...");
  if (mqttClient.connect("", "device", BLYNK_AUTH_TOKEN)) {
    Serial.println(" Conectado!");
  } else {
    Serial.print(" Falhou, rc=");
    Serial.println(mqttClient.state());
  }
}

void mqttSetup(){
  mqttClient.setServer("blynk.cloud", 1883);
  mqttClient.setCallback(mqttCallback);
  mqttReconnect();
}

//
// END MQTT
//

void setup() {
  Serial.begin(9600);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  meuServo.setPeriodHertz(50);
  meuServo.attach(pinoServo, 500, 2400);
  moverServo(10); // Inicia fechado, bomba desligada, testando com 10 para ver se para de esuentar e travar
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  mqttSetup();
  
  // Roda a função de processamento a cada 10 segundos
  timer.setInterval(10000L, processarDadosSistema);
  
  Serial.println("------------------------------------------");
  Serial.println("PROJETO SMART IRRIGATION - PRONTO");
  Serial.println("Controle: Slider (Meta %) ou Botão (Manual)");
  Serial.println("------------------------------------------");
}

void loop() {
  Blynk.run();
  timer.run();
  mqttReconnect();
  mqttClient.loop();
}
