# ShadowOS
Shadow é um kernel de 64-bit SMP for the PC architecture x86_64.

Ele é o produto de muito estudo realizado em fontes em língua inglesa. Nas
minhas consultas, observei a quase ausência de projetos elaborados por fa-
lantes da língua portuguesa. Os poucos projetos encontrados eram de hobby
kernels muito primários. O OS-Lab(ShadowOS) está longe de completo, mas
possui a estrutura básica que todo estudante de sistemas operacionais pre-
cisaria ter para avançar com rapidez na compreensão desse nincho da ciên-
cia da computação.


Implementado:

.Gerenciador de memória física
.Gerenciador de memória virtual
.Memória heap
.ACPI
-APIC
-IOAPIC
-Gerenciamento do Timer
-keyboard
-VESA
-SMP - Ativação de até 4 cores
-Syscall
-Task/Process
-Scheduler
-Shell;

A implementar:
-Um fileSystem 

Pre-requisitos:

-Sugiro o uso do VSCode como editor
-qemu-system-x86_64

