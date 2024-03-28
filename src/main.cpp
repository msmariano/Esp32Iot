
#include <ArduinoMqttClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Espalexa.h>

char ssid[] = "Marianos_B";
char pass[] = "20061977";
String uuid = "9b8068f7-ba15-43cd-a993-2671b1c60aac";
String nick = "LuzFundos";
const char broker[] = "f897f821.ala.us-east-1.emqxsl.com";
String usuarioBroker = "neuverse";
String senhaBroker = "M@r040370";
WiFiClientSecure wifiClient;
MqttClient mqttClient(wifiClient);
void onMqttMessage(int messageSize);
void WiFiEvent(WiFiEvent_t event);
void eventStationModeConnected(WiFiEventStationModeConnected event);
void eventStationModeDisconnected(WiFiEventStationModeDisconnected event);
int totalDia = 0;
int timeAlive = 0;
int timeUpdate = 0;
bool aliveOk = true;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000);
Espalexa espalexa;

int port = 8883;
DynamicJsonDocument buttonIOTs(2048);
int rele = D3;
int ledVerde = D6;
int ledAmarelo = D5;
int ledVermelho = D4;
int interruptor = D7;
bool statusLedVm = LOW;
int estado = LOW;
bool mqttConected = false;
int ultimoAcionamento = 0;
bool temEvento = false;

void IRAM_ATTR isr()
{
  if ((millis() - ultimoAcionamento) >= 250)
  {
     Serial.println("Evento de interrupcao");
    int status = digitalRead(rele);
    if (status == 1)
    {
      digitalWrite(rele, LOW);
      digitalWrite(ledVerde, LOW);
    }
    else
    {
      digitalWrite(rele, HIGH);
      digitalWrite(ledVerde, HIGH);
    }
    temEvento = true;
    ultimoAcionamento = millis();
  }
}

void acionar(uint8_t brightness){

  
  Serial.println("Evento Alexa ");
  if (brightness == 255)
  {
    digitalWrite(rele, HIGH);
  }
  else
  {
    digitalWrite(rele, LOW);  
  }
  String json = "";
  serializeJson(buttonIOTs, json);    
}

void startAlexa()
{
  espalexa.addDevice("Luz fundos", acionar);
  espalexa.begin();
}

void IRAM_ATTR funcaoInterrupcao(){

  if(digitalRead(D7)!=estado){
    if( buttonIOTs[0]["dispositivos"][0]["status"] == "OFF"){
      buttonIOTs[0]["dispositivos"][0]["status"] = "ON";
      digitalWrite(ledVerde, HIGH);
      digitalWrite(rele, HIGH);
    }
    else{
      buttonIOTs[0]["dispositivos"][0]["status"] = "OFF";
      digitalWrite(ledVerde, LOW);
      digitalWrite(rele, LOW);
    }
    String json = "";
    serializeJson(buttonIOTs, json);
    mqttClient.beginMessage("br/com/neuverse/servidores/events");
    mqttClient.print(json);
    mqttClient.endMessage();
    Serial.println("Enviando evento:"+json);
  }
  estado = digitalRead(D7);  



}

void setup()
{

  Serial.begin(115200);
  /*while (!Serial)
  {
  }*/

  pinMode(ledVerde, OUTPUT);
  digitalWrite(ledVerde, LOW);
  pinMode(rele, OUTPUT);
  digitalWrite(rele, LOW);
  pinMode(ledAmarelo, OUTPUT);
  digitalWrite(ledAmarelo, LOW);
  pinMode(ledVermelho, OUTPUT);
  digitalWrite(ledVermelho, LOW);
  pinMode(interruptor, INPUT);
  estado = digitalRead(D7);

  attachInterrupt(interruptor,isr,CHANGE);

  //attachInterrupt(digitalPinToInterrupt(interruptor),funcaoInterrupcao,CHANGE);

  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  //WiFi.onEvent(WiFiEvent);
  WiFi.onStationModeConnected(eventStationModeConnected);
  WiFi.onStationModeDisconnected(eventStationModeDisconnected);
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(ledVermelho, HIGH);
    Serial.print(".");
    delay(500);
    digitalWrite(ledVermelho, LOW);
    delay(500);
  }
  //String smac  = WiFi.macAddress();
  //smac.replace(":","-");
  digitalWrite(ledVermelho, HIGH);
  Serial.println("You're connected to the network");
  Serial.println("Seu MacAddress:"+WiFi.macAddress());
  //uuid = uuid+"-MM"+smac+"-MM-NN"+nick+"NN";
  timeClient.begin();

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  mqttClient.setUsernamePassword(usuarioBroker, senhaBroker);
  mqttClient.setId(uuid);
  mqttClient.setCleanSession(true);
  wifiClient.setInsecure();

  if (!mqttClient.connect(broker, port))
  {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

  }
  else{
    Serial.println("You're connected to the MQTT broker!");
    Serial.println();
    mqttClient.onMessage(onMqttMessage);
    mqttClient.subscribeQoS();
    mqttClient.subscribe("br/com/neuverse/geral/info");
    mqttClient.subscribe("br/com/neuverse/servidores/" + String(uuid) + "/#");
    digitalWrite(ledAmarelo, HIGH);
    mqttConected = true;
   }

  buttonIOTs[0]["nick"] = "Matinhos";
  buttonIOTs[0]["id"] = uuid;
  buttonIOTs[0]["dispositivos"][0]["id"] = 1;
  buttonIOTs[0]["dispositivos"][0]["status"] = "OFF";
  buttonIOTs[0]["dispositivos"][0]["idPool"] = uuid;
  buttonIOTs[0]["dispositivos"][0]["nivelAcionamento"] = "HIGH";
  buttonIOTs[0]["dispositivos"][0]["nick"] = "Luz fundos";
  buttonIOTs[0]["dispositivos"][0][""] = "";
  buttonIOTs[0]["dispositivos"][0]["genero"] = "INTERRUPTOR";

  String json = "";
  serializeJson(buttonIOTs, json);

  Serial.println(json);

  startAlexa();


}

void loop()
{
  espalexa.loop();
  //funcaoInterrupcao();
  mqttClient.poll();
  if(temEvento){
    temEvento = false;
    String json = "";
    serializeJson(buttonIOTs, json);
    Serial.println("Enviando evento:"+json);
    mqttClient.beginMessage("br/com/neuverse/servidores/events");
    mqttClient.print(json);
    mqttClient.endMessage();
  }
  if(totalDia > 172800){
    //ESP.reset();
    totalDia = 0;
  }
  if(aliveOk){
    if(timeAlive > 20){
      timeAlive = 0;
      aliveOk = false;
      mqttClient.beginMessage("br/com/neuverse/servidores/" + String(uuid) + "/alive");
      mqttClient.print("alive");
      mqttClient.endMessage();
    }
  }
  else{      
    if(timeAlive > 10){
      Serial.println("Alive nao recebido. reconectando...");
      timeAlive = 0;
      if(!aliveOk){
        if (!mqttClient.connect(broker, port))
        {
          Serial.print("MQTT connection failed! Error code = ");
          Serial.println(mqttClient.connectError());        
        }
        else{
          Serial.println("You're connected to the MQTT broker!");
          Serial.println();
          mqttClient.onMessage(onMqttMessage);
          mqttClient.subscribeQoS();
          mqttClient.subscribe("br/com/neuverse/geral/info");
          mqttClient.subscribe("br/com/neuverse/servidores/" + String(uuid) + "/#");
          digitalWrite(ledAmarelo, HIGH);
          mqttConected = true;
          mqttClient.beginMessage("br/com/neuverse/servidores/" + String(uuid) + "/alive");
          mqttClient.print("alive");
          mqttClient.endMessage();
        }
      }
    }
  }
  if(timeUpdate>20){
    timeClient.update();
    timeUpdate = 0;
    if(timeClient.getHours() == 0 && timeClient.getMinutes() == 0){
       ESP.reset();
    }
    Serial.println(timeClient.getFormattedTime());
  }
  timeUpdate++;
  timeAlive++;
  totalDia++;
  delay(500);
}

void onMqttMessage(int messageSize)
{
  Serial.println("Received a message with topic '");
  String topico = mqttClient.messageTopic();
  Serial.print(topico);
  String msg = mqttClient.readString();
  Serial.println(mqttClient.messageTopic());
  Serial.println(msg);
  if (topico.equals("br/com/neuverse/geral/info"))
  {
    String json = "";
    serializeJson(buttonIOTs, json);
    mqttClient.beginMessage("br/com/neuverse/geral/lista");
    mqttClient.print(json);
    mqttClient.endMessage();
  }
  if (topico.equals("br/com/neuverse/servidores/" + String(uuid) + "/atualizar"))
  {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, msg);
    for (int i = 0; i < (int)doc.size(); i++)
    {
      for (int j = 0; j < (int)doc[i]["dispositivos"].size(); j++)
      {
        if (doc[i]["dispositivos"][j]["status"] == "ON")
        {
          //estado = true;
          buttonIOTs[i]["dispositivos"][j]["status"] = "ON";
          digitalWrite(ledVerde, HIGH);
          digitalWrite(rele, HIGH);
        }
        else if (doc[i]["dispositivos"][j]["status"] == "OFF")
        {
          //estado = false;
          buttonIOTs[i]["dispositivos"][j]["status"] = "OFF";
          digitalWrite(ledVerde, LOW);
          digitalWrite(rele, LOW);
        }
        else if (doc[i]["dispositivos"][j]["status"] == "PUSHON"){
          digitalWrite(rele, HIGH);
          delay(1000);
          digitalWrite(rele, LOW);
          doc[i]["dispositivos"][j]["status"] = "PUSHOFF";
        } 
      }
    }
    String json = "";
    serializeJson(buttonIOTs, json);
    mqttClient.beginMessage("br/com/neuverse/servidores/events");
    mqttClient.print(json);
    mqttClient.endMessage();
    Serial.println(json);
  }
  if(topico.equals("br/com/neuverse/servidores/" + String(uuid) + "/alive")){
    timeAlive = 0;
    aliveOk = true;
  }

}

void eventStationModeConnected(WiFiEventStationModeConnected event){
  digitalWrite(ledVermelho,HIGH);
}
void eventStationModeDisconnected(WiFiEventStationModeDisconnected event){
  digitalWrite(ledVermelho,LOW);
}

void WiFiEvent(WiFiEvent_t event)
{
  switch(event){
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      digitalWrite(ledVermelho,LOW);
      break;
    case WIFI_EVENT_STAMODE_CONNECTED:
      digitalWrite(ledVermelho,HIGH);
      break;
    case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED:
    case WIFI_EVENT_MODE_CHANGE:
    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
    case WIFI_EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP:
    case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE:
    case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
    case WIFI_EVENT_STAMODE_GOT_IP:
    case WIFI_EVENT_STAMODE_DHCP_TIMEOUT:
    case WIFI_EVENT_MAX:
      break;
  }
}
