Relatório do 1º trabalho de laboratorial

Autores:
João Miguel Ferreira de Araújo Pinto Correia up201905892
Amanda de Oliveira Silva up201800698

Sumário:
O trabalho tem como desenvolvimento uma programação capaz de criar um programa que transmita informação do transmissor para o recetor de forma correta de maneira a ambos conseguirem obter e reter a mesma informação.
Foi concluida a finalidade desse desenvolvimento com a possibilidade de transmitir informações com o uso do nosso programa mesmo com algumas interferências criadas manualmente, os testes usados para verificar o programa.

1. Introdução

Este trabalho tem como objetivos implementar um protocolo de ligação de dados e testar este com uma aplicação simples de transferência de ficheiros. Juntamente com a introdução haverão mais 8 pontos de conteúdo neste relatório:
    • Arquitetura: blocos funcionais e interfaces;
    • Estrutura do código: APIs, principais estruturas de dados, principais funções e a sua relação com a arquitetura;
    • Casos de uso principais: identificação, sequências de chamada de funções;
    • Protocolo de ligação lógica: identificação dos principais aspetos funcionais;
    • Protocolo de aplicação: identificação dos principais aspetos funcionais;
    • Validação: descrição dos testes efetuados com apresentação quantificada dos resultados, se possível;
    • Eficiência do protocolo de ligação de dados: caracterização estatística da eficiência do protocolo, efetuada recorrendo a medidas sobre o código desenvolvido;
    • Conclusões: reflexão sobre os objetivos de aprendizagem alcançados.
Com este relatório cobriremos todos estes tópicos e será realizado a explicação dos passos tomados de maneira a desenvolver uma síntese capaz de explicar o que é resolvido e aprendido neste trabalho laboratorial.

2. Arquitetura

3.Estrutura do código

4.Casos de uso principais

5.Protocolo de ligação lógica

6.Protocolo de aplicação

7.Validação

Os testes efectuados foram os seguintes:
    • Teste normal de verificação se o funcionaria a transmição de dados sem nenhuma interrupção ou interferência – transmite os dados sem qualquer problema e sem nenhum erro durante o processo;
    • Teste com com dois alarmes antes de voltar a ligar a porta série – demonstrou a mensagem de que o caminho para a transmissão de dados estava fechada com o alarme sem problema e depois do segundo alarme foi transmitidos os dados corretamente;
    • Teste com a porta série fechada inicialmente e depois aberta a partir do primeiro alarme – foi mostrado a mensagem do alarme e depois de aberta a porta o program conseguiu transmitir o dados sem problema;
    • Teste na mudança do tamanho dos blocos de informação na camada de aplicação – teste foi falhado uma vez que no código não foi posto a possibilidade de testar isso;
    • Teste de acréscimo de ruído na transmissão – foi detetado uma maior dificuldade a transmitir criando dados defeituosos/errados mas na verificação do BBC isso foi visto e feita a retransmissão do pacote de dados dando finalmente os dados corretamente;

8.Eficiência do protocolo de ligação de dados

9.Conclusões
