##Mercado V1

O programa vai abrir os arquivos auxiliares dentro do diretório. E para cada item dentro dessas listas o programa vai atribuir um valor aleatório. O usuário pode adicionar ou remover itens da lista à vontade.
Uma vez com os valores de estoque sorteados, 3 consumidores vão entrar em ação, eles vão ler os arquivos, ver o que tem estoque e vão escrever um pedido no buffer de tamanho 10, o ato de escrita sendo a zona crítica. Com alguma coisa no buffer o consumidor começa a trabalha esvaziando o estoque mediante o pedido e limpando o buffer, sendo o ato de limpar o buffer uma outra zona crítica.

##Mercado V2
A diferença entre a versão 1 para a 2, é o número de consumidores.

##Mercado V3
A versão 3 é a versão 1, mas sem a proteção das zonas críticas.
