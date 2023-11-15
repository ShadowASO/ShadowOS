# ShadowOS
Shadow é um kernel de 64-bit SMP para a arquitetura x86_64.

Ele é o produto de muito estudo e consultas realizadas principalmente em
fontes produzidas em língua inglesa. 

Nasminhas consultas, observei que a quase totalidade de projetos disponí-
veis estão documentados por falantes da língua portuguesa. Os poucos pro-
jetos brasileiros encontrados eram de hobby kernels muito primários. 

O OS-Lab(ShadowOS) está longe de completo, mas possui a estrutura básica 
que todo estudante de sistemas operacionais precisaria ter para avançar 
com rapidez na compreensão desse nincho da ciência da computação.


## Implementado:

+ Gerenciador de memória física
+ Gerenciador de memória virtual
+ Memória heap
+ ACPI
+ APIC
+ IOAPIC
- Gerenciamento do Timer
- keyboard
- VESA
- SMP - Ativação de até 4 cores
- Syscall
- Task/Process
- Scheduler
- Shell

## A implementar:
-Um fileSystem 

## PRÉ-REQUISITOS:
-GCC 13.2.0 ou superior
-NASM 2.16.01
-qemu-system-x86_64

## Como executar:
Fazer o download do fonte e restaurá-lo em sua máquina de uso.
$ make run - Faz a compilação e executa o kernel, utilizando QEMU

Pre-requisitos:

-Sugiro o uso do VSCode como editor
-qemu-system-x86_64

