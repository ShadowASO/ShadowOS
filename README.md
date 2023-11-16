# ShadowOS
ShadowOS é um kernel de 64-bit com capacidade SMP para a arquitetura x86_64.

Ele é o produto de muita pesquisa realizada em outros kernels e o estudo 
teórico nas obras disponíveis, em sua maioria escritos em língua inglesa.

Nas pesquisas e consultas realizadas, observei que a quase totalidade dos
projetos disponíveis estão documentados na língua inglesa, o que dificulta
a compreensão daqueles que não dominal a língua universa da tecnologia. Os 
poucos projetos brasileiros que encontrei iniciavam e morriam ainda no es-
tágio embrionário.

O OS-Lab(ShadowOS) está longe de completo, mas possui muitos recursos que 
são encontrados nos kernels disponíveis na internet.

Este código, que procurei documentar onde possível, pode ser o trampolim
para todo estudante de sistemas operacionais avançar com rapidez na com-
preensão desse nincho da ciência da computação.


## Implementado:

+ Gerenciador de memória física
+ Gerenciador de memória virtual
+ Memória heap
+ ACPI
+ APIC
+ IOAPIC
+ Gerenciamento do Timer
+ keyboard
+ VESA
+ SMP - Ativação de até 4 cores
+ Syscall
+ Task/Process
+ Scheduler
+ Shell

## A implementar:
O kernel ainda precisa de muitas melhorias e ajustes, e realizar isso ajudará na compreensão dos
conceitos de sistemas operacionais, arquitetura do x86_64

+ Um fileSystem 

## PRÉ-REQUISITOS:

+ GCC 13.2.0 ou superior
+ NASM 2.16.01
+ qemu-system-x86_64

## Como executar:
Faça o download do source code e, após restaurá-los em seu computador,  dentro da pasta principal(ShadowOS),
execute o seguinte comando:

$ make run - Faz a compilação e executa o kernel, utilizando QEMU


