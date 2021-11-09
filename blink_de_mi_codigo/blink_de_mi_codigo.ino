#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "ESP32_MailClient.h"



//***************************************
//*** MQTT CONFIG (conexión al broker)***
//***************************************
const char *mqtt_server = "node02.myqtthub.com";
const int mqtt_port = 1883;
const char *mqtt_user = "esp12E";
const char *mqtt_pass = "esp32";
const char *root_topic_subscribe = "Temperatura/esp32";
const char *root_topic_publish = "Temperatura/public_esp32";


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
  // Corremos sistema de temperatura
  float temp = sistemaTemperatura();

  if(temp == -1.0 ){return;}
  
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

float sistemaTemperatura(){
    // Esperamos 1 min entre medidas
  delay(1000);
 
  // Leemos la humedad relativa
  float h = dht.readHumidity();
  
  // Leemos la temperatura en grados centígrados (por defecto)
  float t = dht.readTemperature();

 
  // Comprobamos si ha habido algún error en la lectura
  if (isnan(h) || isnan(t)) {
    Serial.println("Error obteniendo los datos del sensor DHT11");
    return -1.0;
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
  return t;
}

void sistemaVentilacion(float temperatura){
  if(temperatura < 27){
      digitalWrite(bombilla,HIGH);
      digitalWrite(ventilador,LOW);
      Serial.println();
      Serial.println("TEMP BAJA");
      return;
    }
  if(temperatura >= 29){
    
      digitalWrite(bombilla,LOW);
      digitalWrite(ventilador,HIGH);
      Serial.println();
    Serial.println("TEMP ALTA");
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
  
