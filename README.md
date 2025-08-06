# DeskBuddy

**DeskBuddy** é um projeto pessoal de robô de mesa inteligente criado para trazer interatividade e diversão ao ambiente de trabalho. Ele exibe olhos animados em um display, expressando emoções de acordo com suas interações, humor, energia e relações sociais. DeskBuddy se comunica com outros DeskBuddies via BLE, pode ser monitorado/controlado por um app mobile, e conta com interface web local para consulta de informações.

---

## Principais Funcionalidades

- **Expressão de emoções dinâmicas**: Mostra estados emocionais como normal, feliz, bravo, triste, entediado, suspeita e outros, alternando expressões conforme estímulos, tempo e interação.
- **Animação física**: Movimenta uma antena (ou acessório) por meio de um servo motor sincronizado com as emoções.
- **Interação BLE**: Detecta outros DeskBuddies próximos, identifica e armazena relações (amizade, namoro, antipatia), troca nomes e humores.
- **App Flutter**: Aplicativo para escanear, emparelhar e visualizar informações (emoções, nome, status, relações, bateria), além de ajustes e configurações.
- **Interface Web via WiFi**: Permite acessar informações e configurar o DeskBuddy pela rede local.
- **Persistência em EEPROM**: Armazena dados como relações, senhas e configurações.
- **Controle por botão e sensor**: Mudança de humor por toque ou movimento (com acelerômetro); customização via comandos seriais (modo teste).
- **Monitoramento de bateria**: Mede e exibe a porcentagem de bateria em tempo real.
- **Personalização**: Nome, senha BLE, animações e possíveis temas para múltiplos DeskBuddies.
- **Comunicação segura**: Senha BLE configurável para emparelhamento/autenticação.

---

## Componentes Utilizados

- **ESP32 (S3 Mini ou similar)**: Cérebro do DeskBuddy.
- **Display OLED 128x64**: Exibição das expressões faciais (Adafruit_SSD1306).
- **Servo Motor 9g**: Movimentação da antena/acessório.
- **Bateria Lítio (3.7V) + circuito Boost para 5V**: Autonomia e portabilidade.
- **Módulo TP4056**: Carregamento da bateria.
- **Sensor de tensão**: Monitoramento da carga da bateria.
- **Botão touch/capacitivo**: Para interação.
- **Acelerômetro (opcional, ex: MPU6050, LIS3DH)**: Detecção de movimento.
- **Buzzer (opcional)**: Alertas sonoros.
- **EEPROM**: Armazenamento de dados.
- **WiFi/BLE**: Comunicação local e integração com app.

---

## Estados Emocionais

| Estado      | Imagem                      | Descrição                                             |
|-------------|-----------------------------|-------------------------------------------------------|
| Normal      | ![](assets/normal.gif)      | Olhos piscando normalmente; estado padrão             |
| Feliz       | ![](assets/happy.gif)       | Olhos grandes/brilhantes, sorriso, antena balança     |
| Suspeita    | ![](assets/suspicion.gif)   | Olhos semicerrados, movimento lateral                 |
| Triste      | ![](assets/sad.gif)         | Olhos caídos, expressão de melancolia                 |
| Bravo       | ![](assets/angry.gif)       | Olhos vermelhos, expressão raivosa, antena bate       |
| Entediado   | ![](assets/bored.gif)       | Olhos semicerrados, piscadas lentas                   |

> Outros estados podem ser adicionados e personalizados.

---

## Interações Sociais via BLE

- **Descoberta automática**: DeskBuddy faz scan BLE contínuo e identifica outros por nome e senha.
- **Relações**: Amizade, namoro, antipatia são salvas em EEPROM e influenciam as reações.
- **Envio/Recebimento de informações**: Nome, emoções e status são compartilhados no encontro.
- **Segurança**: Emparelhamento exige senha configurável.

---

## Aplicativo Mobile (Flutter)

- **Conexão BLE**: App escaneia DeskBuddies próximos e emparelha via senha.
- **Exibe informações**: Emoções, nome, status, porcentagem da bateria, relações sociais.
- **Ajuste de configurações**: Possível alterar nome, senha, animações, etc.
- **Notificações**: Alertas quando DeskBuddy precisa de atenção.

---

## Interface Web WiFi

- **Dashboard amigável**: Acesso pelo navegador via IP local.
- **Informações exibidas**: Nome, emoções, bateria, conexões BLE/WiFi, ajustes avançados.
- **Configuração remota**: Ajustes rápidos pelo browser.

---

## Desenvolvimento

- **Firmware em C++ (Arduino IDE)**: Lógica principal, controle de display, servo, BLE, WiFi, armazenamento EEPROM.
- **App Flutter (Dart)**: Interface mobile multiplataforma.
- **Design e prototipagem**: Modelos 3D para case, acessórios e peças customizadas (STL disponíveis no repositório).
- **Documentação**: Diagramas, imagens de montagem, exemplos de uso.

---

## Como Usar

1. **Monte o hardware** conforme a lista de componentes.
2. **Carregue o firmware** no ESP32 (veja pasta `/firmware`).
3. **Emparelhe com o app Flutter** para gerenciar e customizar.
4. **Acesse a interface web** via IP local para monitorar status.
5. **Interaja**: Toque, mova ou aproxime de outros DeskBuddies!

---

## Objetivos do Projeto

- Tornar o ambiente de trabalho mais divertido e interativo.
- Estimular aprendizado de eletrônica, BLE, WiFi e integração mobile.
- Permitir personalização e expansão por outros makers/devs.
- Desenvolver um ecossistema de robôs de mesa sociais.

---

## Futuro do Projeto

- Novos estados emocionais, animações e sons.
- Acesso remoto via nuvem e integração com assistentes virtuais (ex: Alexa, Google Home).
- Mini-jogos e interações contextuais entre DeskBuddies.
- Melhoria da IA de detecção de humor e contexto (baseado em eventos do ambiente).
- Suporte a novos sensores e acessórios.
