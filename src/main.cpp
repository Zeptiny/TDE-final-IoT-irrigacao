#define BLYNK_TEMPLATE_ID   "TMPL2WODQMGv3"
#define BLYNK_TEMPLATE_NAME "TRABALHO FINAL IOT"
#define BLYNK_AUTH_TOKEN    "nlhG7jxJfG5u9qFkqS6YgAwSbYnfMIdr"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>

// Credenciais da rede
char ssid[] = "Airton Sala"; 
char pass[] = "20303009rosset"; 

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
  int valorBruto = analogRead(pinoSensor);
  // Converte a leitura analógica para porcentagem (0 a 100%)
  umidadePercentual = map(valorBruto, 4095, 0, 0, 100); 
  
  // Atualiza o Gauge (V1) no celular
  Blynk.virtualWrite(V1, umidadePercentual);

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

void setup() {
  Serial.begin(115200);
  
  meuServo.attach(pinoServo);
  meuServo.write(0); // Inicia fechado, bomba desligada
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
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
}