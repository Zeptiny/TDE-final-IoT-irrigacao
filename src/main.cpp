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
char ssid[] = "A25 de Gregori"; 
char pass[] = "09111218Pichu"; 

// Objetos
Servo meuServo;
BlynkTimer timer;

// Pinos
const int pinoSensor = 34; 
const int pinoServo  = 18; 

// Variáveis de Controle
bool modoManualForcado = false; // Status do botão V2
int umidadePercentual = 0;      // Leitura atual do sensor
int limiteUmidade = 0;          // Meta de umidade escolhida no Slider (V3)

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
      meuServo.write(90); // Abre totalmente a bomba no manual
      Serial.println("MODO MANUAL ATIVADO: Bomba ligada via App.");
    } else {
      meuServo.write(0);  // Fecha a bomba
      Serial.println("MODO MANUAL DESATIVADO: Bomba desligada.");
    }
  }
}

// PROCESSAMENTO: Leitura e Decisão Automática
void processarDadosSistema() {
  // 1. Leitura do Sensor
  // DEBUG: Valor aleatório
  int valorBruto = random(0, 4095);
  //int valorBruto = analogRead(pinoSensor);
  // Converte a leitura analógica para porcentagem (0 a 100%)
  umidadePercentual = map(valorBruto, 4095, 0, 0, 100); 
  Serial.println("Umidade Atual: " + String(umidadePercentual) + "%");
  
  // Atualiza o Gauge (V1) no celular
  //Blynk.virtualWrite(V1, umidadePercentual);
  if (mqttClient.connected()) {
    if (mqttClient.publish("ds/umidade", String(umidadePercentual).c_str())) {
      Serial.println("MQTT: Umidade enviada com sucesso.");
    } else {
      Serial.println("MQTT: Falha ao enviar umidade.");
    }
  } else {
    Serial.println("MQTT: Cliente não conectado. Não foi possível enviar a umidade.");
  }


  // 2. Lógica de Controle do sistema
  // Se o botão manual estiver ligado, a automação abaixo não roda
  if (modoManualForcado == true) {
    return; 
  }

  // Modo Automático: Só funciona se o Slider estiver acima de 0%
  if (limiteUmidade > 0) {
    // Se a umidade atual for menor que a escolhida no Slider
    if (umidadePercentual < limiteUmidade) {
      // Abre o servo para irrigar
      meuServo.write(90); 
      Serial.print("AUTOMÁTICO: Umidade (");
      Serial.print(umidadePercentual);
      Serial.print("%) abaixo da meta (");
      Serial.print(limiteUmidade);
      Serial.println("%). Irrigando...");
    } else {
      // Se atingiu a meta, fecha
      meuServo.write(0);
      Serial.println("AUTOMÁTICO: Meta atingida. Bomba desligada.");
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


void mqttSetup(){
  mqttClient.setServer("blynk.cloud", 1883);
  // OBS: cleanSession não está ativada, e keepalive não configurado, talvez dê problema
  if (mqttClient.connect("", "device", BLYNK_AUTH_TOKEN)) {
    Serial.println("MQTT Conectado com Sucesso!");

    mqttClient.setCallback(mqttCallback);
  }

}

//
// END MQTT
//

void setup() {
  Serial.begin(9600);
  
  meuServo.attach(pinoServo);
  meuServo.write(0); // Inicia fechado, bomba desligada
  
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
  mqttClient.loop();
}