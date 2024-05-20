# DeskBuddy

**DeskBuddy** é o meu projeto que trará um companheiro robótico para a mesa de trabalho. Este pequeno robô interage através de um display, exibindo olhos expressivos que refletem seus sentimentos e se comunicam com outros DeskBuddies ao seu redor. Com o DeskBuddy, espero transformar qualquer espaço de trabalho em um ambiente mais animado e interativo!

## Funcionalidades

Estou projetando o DeskBuddy para mostrar diferentes estados emocionais através dos seus olhos no display. Abaixo estão os principais estados emocionais que estou desenvolvendo para o DeskBuddy:

- **Normal:** Olhos normais piscando.
- **Happy:** Olhos que refletem felicidade.
- **Suspicion:** Olhos que mostram desconfiança.
- **Sad:** Olhos tristes.
- **Angry:** Olhos zangados.

## Componentes

Para construir o DeskBuddy, estou utilizando os seguintes componentes:

- **ESP32:** Microcontrolador principal que controlará todas as funcionalidades do DeskBuddy.
- **Display 128x64:** Tela para exibir os olhos do DeskBuddy.
- **Bateria:** Fonte de energia portátil para o DeskBuddy.
- **Buzzer:** Para emitir sons e alertas.
- **Módulo para leitura da voltagem da bateria:** Para monitoramento do nível de carga da bateria.
- **Botão touch:** Interface de entrada para o usuário interagir com o DeskBuddy.
- **Acelerômetro (opcional):** Para detectar movimentos e ajustar os estados emocionais do DeskBuddy conforme necessário.
- **BLE (Bluetooth Low Energy):** Para comunicação com outros DeskBuddies.
- **WiFi:** Para conexão à rede e consulta de informações.

## Como funciona

O DeskBuddy usa um display de 128x64 pixels para mostrar olhos animados que mudam conforme o estado emocional do robô. Cada estado emocional é projetado para ser claramente distinguível, tornando fácil para o usuário entender como o DeskBuddy "se sente" em diferentes momentos.

### Interações Sociais

Com a tecnologia BLE, o DeskBuddy poderá detectar e se comunicar com outros DeskBuddies próximos. Quando dois ou mais DeskBuddies se encontrarem, eles poderão interagir de diversas formas:

- **Gostar ou não gostar:** Podem se tornar amigos ou "não gostar" um do outro.
- **Serem amigos:** Exibirão expressões felizes quando estiverem perto.
- **Namorados:** Terão interações especiais e carinhosas entre DeskBuddies "namorados".

### Consulta de Informações

Através da conexão WiFi, será possível acessar o DeskBuddy para consultar diversas informações:

- **Nome do DeskBuddy**
- **Status atual**
- **Porcentagem da bateria**
- **Outras informações relevantes**

Você poderá se conectar ao DeskBuddy através de um navegador web em sua rede local e acessar uma interface amigável para visualizar essas informações.

## Estados Emocionais

1. **Normal:**

![Logo do Markdown](assets/normal.gif)
   - Descrição: Este é o estado padrão do DeskBuddy. Os olhos piscam de forma regular, sem expressar nenhuma emoção específica.
   - Uso: Quando o DeskBuddy não está respondendo a nenhum estímulo particular.

2. **Happy:**

![Logo do Markdown](assets/happy.gif)
   - Descrição: Os olhos do DeskBuddy se alargam e brilham, demonstrando uma expressão de felicidade.
   - Uso: Quando algo positivo acontece ou o DeskBuddy quer expressar satisfação.

3. **Suspicion:**

![Logo do Markdown](assets/suspicion.gif)
   - Descrição: Os olhos do DeskBuddy se estreitam e se movem de um lado para o outro, indicando desconfiança.
   - Uso: Quando algo estranho é detectado ou o DeskBuddy está "curioso" sobre algo.

4. **Sad:**

![Logo do Markdown](assets/sad.gif)
   - Descrição: Os olhos do DeskBuddy se tornam tristes e caídos, com uma expressão melancólica.
   - Uso: Quando o DeskBuddy "percebe" algo negativo ou está "triste" por algum motivo.

5. **Angry:**

![Logo do Markdown](assets/angry.gif)
   - Descrição: Os olhos do DeskBuddy se estreitam e ficam vermelhos, mostrando raiva.
   - Uso: Quando o DeskBuddy "sente" que algo está errado ou está "irritado".

## Desenvolvimento e Tecnologias

Estou desenvolvendo o DeskBuddy utilizando uma combinação de hardware e software:

- **Hardware:**
  - **Display 128x64:** Para mostrar as expressões faciais.
  - **ESP32:** Para controlar o display, estados emocionais, comunicação BLE e WiFi.
  - **Bateria:** Para alimentar o DeskBuddy.
  - **Buzzer:** Para emitir sons e alertas.
  - **Módulo de leitura de voltagem:** Para monitorar o nível de carga da bateria.
  - **Botão touch:** Para interação do usuário.
  - **Acelerômetro (opcional):** Para detecção de movimento e ajuste dos estados emocionais.

- **Software:**
  - Programação em C/C++ utilizando o ambiente de desenvolvimento Arduino IDE.
  - Animações de olhos criadas utilizando gráficos simples para diferentes emoções.
  - Lógica para monitoramento de bateria e resposta a toques e movimentos.
  - Implementação de comunicação BLE para interação entre DeskBuddies.
  - Interface web para consulta de informações via WiFi.

## Objetivos do Projeto

- Criar um robô de mesa interativo e amigável.
- Desenvolver uma interface visual que comunica emoções de forma clara.
- Permitir interação entre múltiplos DeskBuddies usando BLE.
- Proporcionar um companheiro de trabalho que pode trazer um pouco de alegria e interatividade ao ambiente.
- Facilitar a consulta de informações importantes através de uma interface web acessível via WiFi.

## Futuro do Projeto

Estou trabalhando para finalizar um protótipo funcional do DeskBuddy. No futuro, pretendo adicionar mais funcionalidades e refinamentos, como:

- **Mais emoções e animações:** Ampliar a gama de expressões faciais do DeskBuddy.
- **Integração com assistentes virtuais:** Permitir que o DeskBuddy interaja com outros dispositivos inteligentes.
- **Aprimoramento das interações sociais:** Melhorar a complexidade e a naturalidade das interações entre DeskBuddies.

## Contribuições

Contribuições para o projeto são bem-vindas! Se você tiver ideias, sugestões ou quiser ajudar no desenvolvimento, sinta-se à vontade para abrir uma issue ou enviar um pull request.