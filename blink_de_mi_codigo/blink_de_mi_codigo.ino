#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "ESP32_MailClient.h"

//***************************************
//*** Firebase - Injection nodemcu ******
//***************************************
#if defined(ESP32)
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>

/**Helpers - Firebase*/
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

/** Credenciales - Firebase**/
#define API_KEY "AIzaSyC8CDTVvnRGXBvGEnfYutG7CNNn1cboFMw"
#define DATABASE_URL "proyecto-iot-esp32-default-rtdb.firebaseio.com" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

// Define Object Firebase 
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

int  flag_venti;
int  flag_bombilla;


//***************************************
//*** MQTT CONFIG (conexión al broker)***
//***************************************
const char *mqtt_server = "node02.myqtthub.com";
const int mqtt_port = 1883;
const char *mqtt_user = "esp12E";
const char *mqtt_pass = "esp32";

const char *root_topic_subscribe = "Temperatura/esp32";
const char *root_topic_publish = "Temperatura/public_esp32";

const char *root_topic_subscribe1 = "Humedad/esp32";
const char *root_topic_publish1 = "Humedad/public_esp32";

const char *root_topic_subscribe2 = "Door/esp32";
const char *root_topic_publish2 = "Door/public_esp32";


//***************************************
//***** DECLARAMOS VARIABLES (correo)****
//***************************************
SMTPData datosSMTP;
int boton=0;
int cuenta=0;

           

//***************************************
//*** WIFI CONFIG (conexión a la red)****
//***************************************
char ssid[] = "FLIA CENTENO";
const char* password = "SomosUno";



//**************************************
//***  GLOBALES (variables)  ***********
//**************************************
WiFiClient espClient;
PubSubClient client(espClient);
char msg[25];
long count=0;




//************************
//** F U N C I O N E S ***
//************************
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void setup_wifi();



//***************************************
//**** SENSOR HUMEDAD & TEMPERATURA *****
//***************************************
#define DHTPIN 23
#define DHTTYPE DHT11

// Declaramsos variables & pines
int ventilador = 5;
int bombilla = 19;
 
// Inicializamos el sensor DHT11
DHT dht(DHTPIN, DHTTYPE);


void setup() 
{
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  
  /**----------- Configuramos Firebase --------**/

 /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  /**-------- End Configuramos Firebase --------**/


  //DHT11 - configuramos las salidas 
  pinMode(ventilador,OUTPUT);
  pinMode(bombilla,OUTPUT);
  pinMode(22, OUTPUT);//Led en pin 22
  pinMode(12, INPUT);//Pulsador
  
  // Comenzamos el sensor DHT
  dht.begin();
}

void loop() 
{
  // Corremos sistema de temperatura y retorna la data "temp,humd,door"
  String data = sistemaTemperatura();
  if(data == "err,err,err" ){return;}
  

  //******** split data *******//
  float temp = getValue(data,',',0).toFloat();
  float humd = getValue(data,',',1).toFloat();
  int door = getValue(data,',',2).toFloat();
  //***************************//
  
  
  /*** PUBLICAR DATOS EN FIREBASE **/
  Serial.print("Testeando Temperatura:");
  Serial.println(humd);
  // publish_db(float S_temp, float S_hum , int S_door ,int A_ventilador, int A_LuzEntrada, int A_habLuz)

    publish_db(temp,humd,1,flag_venti,0,flag_bombilla);
    delay(500);
  /*** END - PUBLICAR DATOS EN FIREBASE **/
  

  // -----------------------------

  // corremos el envio de emails
  boton = digitalRead(12);
  
  if(boton==0){
  Serial.println();
  Serial.print("Iniciando correo!!!");
  delay(200);
  correo();
  }
  
  // -----------------------------
  
  if (!client.connected())
  {
    reconnect();
  }

  if (client.connected())
  {
    String str = "Publicando Topic: " + String(temp);
    str.toCharArray(msg,25);
    client.publish(root_topic_publish,msg);
    Serial.println();
    Serial.println(msg);

    
    String str1 = "Publicando Topic: " + String(humd);
    str1.toCharArray(msg,25);
    client.publish(root_topic_publish1,msg);
    Serial.println();
    Serial.println(msg);


    String str2 = "Publicando Topic: " + String(door);
    str2.toCharArray(msg,25);
    client.publish(root_topic_publish2,msg);
    Serial.println();
    Serial.println(msg);

    
    delay(5000);
  }
  client.loop();
}



//*****************************
//***    CONEXION WIFI      ***
//*****************************
void setup_wifi()
{
  delay(5000);
  // Nos conectamos a nuestra red Wifi
  Serial.println();
  Serial.print("Conectando a ssid: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  Serial.println(WiFi.status());
  
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Conectado a red WiFi!");
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());
}



//******************************************
//*** CONEXION MQTT (conexión al broker) ***
//******************************************
void reconnect() 
{
  while (!client.connected()) 
  {
    Serial.print("Intentando conectar al Broker...");
    // Creamos un cliente ID
    String clientId = "Micro_esp";
    
    // Intentamos conectar
    if (client.connect(clientId.c_str(),mqtt_user,mqtt_pass))
   {
      Serial.println("Conectado al broker!");

      // Nos suscribimos
      if(client.subscribe(root_topic_subscribe))
      {
        Serial.println("Suscripción a topic "+ String(root_topic_subscribe));
      }
      else
      {
        Serial.println("fallo Suscripción a topic "+ String(root_topic_subscribe));
      }
    } 
    else 
    {
      Serial.print("falló conexión a broker: (con error -> ");
      Serial.print(client.state());
      Serial.println(" Intentamos de nuevo en 5 segundos");
      delay(5000);
    }
  }
}



//*****************************
//***       CALLBACK        ***
//*****************************
void callback(char* topic, byte* payload, unsigned int length)
{
  String incoming = "";
  Serial.print("Mensaje recibido desde -> ");
  Serial.print(topic);
  Serial.println("");
  for (int i = 0; i < length; i++)
  {
    incoming += (char)payload[i];
  }
  incoming.trim();
  Serial.println("Mensaje -> " + incoming);
}



//*****************************
//***       DHT11           ***
//*****************************

String sistemaTemperatura(){
    // Esperamos 1 min entre medidas
  delay(1000);
 
  // Leemos la humedad relativa
  float h = dht.readHumidity();
  
  // Leemos la temperatura en grados centígrados (por defecto)
  float t = dht.readTemperature();

   // Comprobamos si ha habido algún error en la lectura
  if (isnan(h) || isnan(t)) {
    Serial.println("Error obteniendo los datos del sensor DHT11");
    return "err,err,err";
  }


  sistemaVentilacion(t);
 
  // Calcular el índice de calor en grados centígrados
  float hic = dht.computeHeatIndex(t, h, false);
 
  Serial.print("Humedad: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperatura: ");
  Serial.print(t);
  Serial.print(" °C ");
  Serial.println();


  //flag
  String d = "1";
  String te ="";
  String hu ="";
  te.concat(t);
  hu.concat(h);
  String data = strJoin(te,hu,d);
  
  Serial.print("test:");
  Serial.println(data);
  return data;
}

String strJoin(String v1,String v2,String v3){
  String coma =",";
  String lg = v1+coma+v2+coma+v3;
return lg;  
}

void sistemaVentilacion(float temperatura){
  if(temperatura < 27){
      digitalWrite(bombilla,HIGH);
      digitalWrite(ventilador,LOW);
      Serial.println();
      Serial.println("TEMP BAJA");
      flag_venti = 0;
      flag_bombilla = 1;
      return;
    }
  if(temperatura >= 29){
    
      digitalWrite(bombilla,LOW);
      digitalWrite(ventilador,HIGH);
      Serial.println();
    Serial.println("TEMP ALTA");
    
      flag_venti = 1;
      flag_bombilla = 0;
    return;
  }
}



//****************************************
//*** EMAIL CONFIG (conexión al correo)***
//****************************************
// Para enviar correos electrónicos usando Gmail, use el puerto 465 (SSL) y el servidor SMTP smtp.gmail.com
//Hay que habilitar la opción de aplicación menos segura https://myaccount.google.com/u/1/security
// El objeto SMTPData contiene configuración y datos para enviar

void correo(){
digitalWrite(22, HIGH);

//Configuración del servidor de correo electrónico SMTP, host, puerto, cuenta y contraseña
datosSMTP.setLogin("smtp.gmail.com", 465, "electivaiotunisangil@gmail.com", "ElectivaIOT20");

// Establecer el nombre del remitente y el correo electrónico
datosSMTP.setSender("Lab3_ESP32", "johcen182@hotmail.com");

// Establezca la prioridad o importancia del correo electrónico High, Normal, Low o 1 a 5 (1 es el más alto)
datosSMTP.setPriority("High");

// Establecer el asunto
datosSMTP.setSubject("Envio evidencias Laboratorio 3, terminado.");

// Establece el mensaje de correo electrónico en formato de texto (sin formato)
datosSMTP.setMessage("Hola somos el grupo de Johan, Harold & Eliana y nos estamos reportando", false);

// Agregar destinatarios, se puede agregar más de un destinatario
datosSMTP.addRecipient("haroldbarrera@unisangil.edu.co");
 
// Comience a enviar correo electrónico.
if (!MailClient.sendMail(datosSMTP))
Serial.println("Error enviando el correo, " + MailClient.smtpErrorReason());

// Borrar todos los datos del objeto datosSMTP para liberar memoria
datosSMTP.empty();
delay(10000);
digitalWrite(22, LOW);
}
  



//****************************************
//*** Dividir string***
//****************************************

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}



//*****************************************
//******** FIREBASE Publish - Data ********
//*****************************************

void publish_db(float S_temp, float S_hum , int S_door ,int A_ventilador, int A_LuzEntrada, int A_habLuz){
  
  String path = "Proy-IoT/";
  
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    
  Serial.print("bombilla:");
  Serial.println(A_habLuz);
  Serial.print("ventilador:");
  Serial.println(A_ventilador);
    
    /*** PUBLIC SENSORES **/
    // Creamos el path "/Proy-IoT/Sensores/Humedad"
    if (Firebase.RTDB.setFloat(&fbdo, path + "Sensores/humedad", S_hum)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
    
    // Creamos el path "/Proy-IoT/Sensores/Temperatura"
    if (Firebase.RTDB.setFloat(&fbdo, path + "Sensores/temperatura", S_temp)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
    
    // Creamos el path "/Proy-IoT/Sensores/proximidad"
    if (Firebase.RTDB.setInt(&fbdo, path + "Sensores/proximidad", S_door)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    /*** PUBLIC ACTUADORES -string output **/
    if(A_LuzEntrada == 1){
      
        // Creamos el path "/Proy-IoT/Actuadores/puerta"
        if (Firebase.RTDB.setString(&fbdo, path + "Actuadores/entrada", "Encendida")){
          Serial.println("PASSED");
          Serial.println("PATH: " + fbdo.dataPath());
          Serial.println("TYPE: " + fbdo.dataType());
        }
        else {
          Serial.println("FAILED");
          Serial.println("REASON: " + fbdo.errorReason());
        }
      }else{
        // Creamos el path "/Proy-IoT/Actuadores/puerta"
        if (Firebase.RTDB.setString(&fbdo, path + "Actuadores/entrada", "Apagada")){
          Serial.println("PASSED");
          Serial.println("PATH: " + fbdo.dataPath());
          Serial.println("TYPE: " + fbdo.dataType());
        }
        else {
          Serial.println("FAILED");
          Serial.println("REASON: " + fbdo.errorReason());
        }
    }

    if(A_ventilador == 1){
          // Creamos el path "/Proy-IoT/Actuadores/ventilador"
          if (Firebase.RTDB.setString(&fbdo, path + "Actuadores/ventilador", "Encendido")){
            Serial.println("PASSED");
            Serial.println("PATH: " + fbdo.dataPath());
            Serial.println("TYPE: " + fbdo.dataType());
          }
          else {
            Serial.println("FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
          }
    }else {
            
        // Creamos el path "/Proy-IoT/Actuadores/habPrincipal"
        if (Firebase.RTDB.setString(&fbdo, path + "Actuadores/ventilador", "Apagado")){
          Serial.println("PASSED");
          Serial.println("PATH: " + fbdo.dataPath());
          Serial.println("TYPE: " + fbdo.dataType());
        }
        else {
          Serial.println("FAILED");
          Serial.println("REASON: " + fbdo.errorReason());
        }
    }

    if(A_habLuz==1){
        // Creamos el path "/Proy-IoT/Actuadores/habPrincipal"
        if (Firebase.RTDB.setString(&fbdo, path + "Actuadores/habPrincipal", "Encendido")){
          Serial.println("PASSED");
          Serial.println("PATH: " + fbdo.dataPath());
          Serial.println("TYPE: " + fbdo.dataType());
        }
        else {
          Serial.println("FAILED");
          Serial.println("REASON: " + fbdo.errorReason());
        }
      
     }else {
        // Creamos el path "/Proy-IoT/Actuadores/habPrincipal"
        if (Firebase.RTDB.setString(&fbdo, path + "Actuadores/habPrincipal", "Apagado")){
          Serial.println("PASSED");
          Serial.println("PATH: " + fbdo.dataPath());
          Serial.println("TYPE: " + fbdo.dataType());
        }
        else {
          Serial.println("FAILED");
          Serial.println("REASON: " + fbdo.errorReason());
        }
     }
  }
  
}


