/*--------------------------------------------------------------------------
*  File name:  idt.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 01-08-2020
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a tabela
IDT e seus descritores, necessários para a manipulação das interrupções.
--------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "stdlib.h"
#include "idt.h"
#include <io.h>
#include "x86_64.h"
#include <screen.h>
#include "mm/page.h"
#include "gdt.h"
#include "interrupt.h"
#include "lapic.h"

// idtd_gate_t idt_table[IDT_ENTRIES];

/* Vetor com todas as IDT utilizáveis. */
extern idt_t idt_percpu[];

/* irá apontar para o  idt do BSP em idt_percpu[].*/
idt_t *idt_table;

/* Faz a inserção de um registro na tabela IDT.*/
static void idt_set_gate(uint8_t vetor, virt_addr_t isr_handler, uint8_t ist, uint8_t type)
{
	// idtd_gate_t *d = (idtd_gate_t *)&idt_table[vetor];
	idtd_gate_t *d = (idtd_gate_t *)&idt_table->entry[vetor];

	/* Atributos. */
	d->selector = _GDT_KERNEL_CODE_SEL; // Kernel code segment
	d->type = type;
	d->ist = ist;

	/* Endereço do segmento.*/
	d->offset0 = BF_GET((mm_addr_t)isr_handler, 0, 16);
	d->offset16 = BF_GET((mm_addr_t)isr_handler, 16, 16);
	d->offset32 = BF_GET((mm_addr_t)isr_handler, 32, 32);
	d->reserved = 0;
}

/*
Faz a inicialização dos 255 registros da tabela IDT. As rotinas "isr_stub_x" foram
todas criadas em assembly, no arquivo isr_low.s
-----------------------------------------
nr_reg | handler | TSS->IST | type_attr
-----------------------------------------
*/

void setup_idt(void)
{
	u8_t id_cpu = cpu_id();

	/* Pego a IDT do BSP no vetor idt_percpu[]*/
	idt_table = &idt_percpu[id_cpu];
	memset(idt_table, 0, sizeof(idt_t));

	idt_set_gate(0, isr_stub_0, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(1, isr_stub_1, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(2, isr_stub_2, IST1_STACK_NMI, TYPE_TRAP);
	idt_set_gate(3, isr_stub_3, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(4, isr_stub_4, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(5, isr_stub_5, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(6, isr_stub_6, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(7, isr_stub_7, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(8, isr_stub_8, IST1_STACK_NMI, TYPE_TRAP);
	idt_set_gate(9, isr_stub_9, IST0_DISABLED, TYPE_TRAP);

	idt_set_gate(10, isr_stub_10, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(11, isr_stub_11, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(12, isr_stub_12, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(13, isr_stub_13, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(14, isr_stub_14, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(15, isr_stub_15, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(16, isr_stub_16, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(17, isr_stub_17, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(18, isr_stub_18, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(19, isr_stub_19, IST0_DISABLED, TYPE_TRAP);

	idt_set_gate(20, isr_stub_20, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(21, isr_stub_21, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(22, isr_stub_22, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(23, isr_stub_23, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(24, isr_stub_24, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(25, isr_stub_25, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(26, isr_stub_26, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(27, isr_stub_27, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(28, isr_stub_28, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(29, isr_stub_29, IST0_DISABLED, TYPE_TRAP);

	idt_set_gate(30, isr_stub_30, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(31, isr_stub_31, IST0_DISABLED, TYPE_TRAP);
	idt_set_gate(32, isr_stub_32, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(33, isr_stub_33, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(34, isr_stub_34, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(35, isr_stub_35, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(36, isr_stub_36, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(37, isr_stub_37, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(38, isr_stub_38, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(39, isr_stub_39, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(40, isr_stub_40, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(41, isr_stub_41, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(42, isr_stub_42, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(43, isr_stub_43, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(44, isr_stub_44, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(45, isr_stub_45, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(46, isr_stub_46, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(47, isr_stub_47, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(48, isr_stub_48, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(49, isr_stub_49, IST0_DISABLED, TYPE_INTE); /* keyboard. */
	idt_set_gate(50, isr_stub_50, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(51, isr_stub_51, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(52, isr_stub_52, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(53, isr_stub_53, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(54, isr_stub_54, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(55, isr_stub_55, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(56, isr_stub_56, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(57, isr_stub_57, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(58, isr_stub_58, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(59, isr_stub_59, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(60, isr_stub_60, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(61, isr_stub_61, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(62, isr_stub_62, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(63, isr_stub_63, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(64, isr_stub_64, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(65, isr_stub_65, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(66, isr_stub_66, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(67, isr_stub_67, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(68, isr_stub_68, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(69, isr_stub_69, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(70, isr_stub_70, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(71, isr_stub_71, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(72, isr_stub_72, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(73, isr_stub_73, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(74, isr_stub_74, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(75, isr_stub_75, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(76, isr_stub_76, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(77, isr_stub_77, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(78, isr_stub_78, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(79, isr_stub_79, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(80, isr_stub_80, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(81, isr_stub_81, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(82, isr_stub_82, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(83, isr_stub_83, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(84, isr_stub_84, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(85, isr_stub_85, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(86, isr_stub_86, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(87, isr_stub_87, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(88, isr_stub_88, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(89, isr_stub_89, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(90, isr_stub_90, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(91, isr_stub_91, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(92, isr_stub_92, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(93, isr_stub_93, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(94, isr_stub_94, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(95, isr_stub_90, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(96, isr_stub_96, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(97, isr_stub_97, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(98, isr_stub_98, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(99, isr_stub_99, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(100, isr_stub_100, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(101, isr_stub_101, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(102, isr_stub_102, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(103, isr_stub_103, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(104, isr_stub_104, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(105, isr_stub_105, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(106, isr_stub_106, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(107, isr_stub_107, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(108, isr_stub_108, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(109, isr_stub_109, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(110, isr_stub_110, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(111, isr_stub_111, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(112, isr_stub_112, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(113, isr_stub_113, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(114, isr_stub_114, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(115, isr_stub_115, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(116, isr_stub_116, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(117, isr_stub_117, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(118, isr_stub_118, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(119, isr_stub_119, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(120, isr_stub_120, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(121, isr_stub_121, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(122, isr_stub_122, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(123, isr_stub_123, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(124, isr_stub_124, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(125, isr_stub_125, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(126, isr_stub_126, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(127, isr_stub_127, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(128, isr_stub_128, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(129, isr_stub_129, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(130, isr_stub_130, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(131, isr_stub_131, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(132, isr_stub_132, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(133, isr_stub_133, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(134, isr_stub_134, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(135, isr_stub_135, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(136, isr_stub_136, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(137, isr_stub_137, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(138, isr_stub_138, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(139, isr_stub_139, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(140, isr_stub_140, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(141, isr_stub_141, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(142, isr_stub_142, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(143, isr_stub_143, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(144, isr_stub_144, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(145, isr_stub_145, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(146, isr_stub_146, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(147, isr_stub_147, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(148, isr_stub_148, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(149, isr_stub_149, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(150, isr_stub_150, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(151, isr_stub_151, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(152, isr_stub_152, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(153, isr_stub_153, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(154, isr_stub_154, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(155, isr_stub_155, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(156, isr_stub_156, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(157, isr_stub_157, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(158, isr_stub_158, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(159, isr_stub_159, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(160, isr_stub_160, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(161, isr_stub_161, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(162, isr_stub_162, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(163, isr_stub_163, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(164, isr_stub_164, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(165, isr_stub_165, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(166, isr_stub_166, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(167, isr_stub_167, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(168, isr_stub_168, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(169, isr_stub_169, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(170, isr_stub_170, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(171, isr_stub_171, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(172, isr_stub_172, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(173, isr_stub_173, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(174, isr_stub_174, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(175, isr_stub_175, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(176, isr_stub_176, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(177, isr_stub_177, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(178, isr_stub_178, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(179, isr_stub_179, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(180, isr_stub_180, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(181, isr_stub_181, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(182, isr_stub_182, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(183, isr_stub_183, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(184, isr_stub_184, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(185, isr_stub_185, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(186, isr_stub_186, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(187, isr_stub_187, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(188, isr_stub_188, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(189, isr_stub_189, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(190, isr_stub_190, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(191, isr_stub_191, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(192, isr_stub_192, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(193, isr_stub_193, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(194, isr_stub_194, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(195, isr_stub_195, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(196, isr_stub_196, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(197, isr_stub_197, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(198, isr_stub_198, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(199, isr_stub_199, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(201, isr_stub_201, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(202, isr_stub_202, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(203, isr_stub_203, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(204, isr_stub_204, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(205, isr_stub_205, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(206, isr_stub_206, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(207, isr_stub_207, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(208, isr_stub_208, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(209, isr_stub_209, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(211, isr_stub_211, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(212, isr_stub_212, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(213, isr_stub_213, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(214, isr_stub_214, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(215, isr_stub_215, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(216, isr_stub_216, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(217, isr_stub_217, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(218, isr_stub_218, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(219, isr_stub_219, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(220, isr_stub_220, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(221, isr_stub_221, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(222, isr_stub_222, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(223, isr_stub_223, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(224, isr_stub_224, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(225, isr_stub_225, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(226, isr_stub_226, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(227, isr_stub_227, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(228, isr_stub_228, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(229, isr_stub_229, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(230, isr_stub_230, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(231, isr_stub_231, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(232, isr_stub_232, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(233, isr_stub_233, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(234, isr_stub_234, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(235, isr_stub_235, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(236, isr_stub_236, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(237, isr_stub_237, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(238, isr_stub_238, IST0_DISABLED, TYPE_INTE);

	/* Este handler é utilizado para as interrupções de tempo produzidas pelo
APIC TIMER e utilizadas pelo scheduler.Passei a usar o IRQ_BASE == 0xEF(239).
*/
	idt_set_gate(ISR_VECTOR_TIMER, isr_stub_239, IST0_DISABLED, TYPE_INTE);
	/*---------------------------------------------------------------------*/

	idt_set_gate(240, isr_stub_240, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(241, isr_stub_241, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(242, isr_stub_242, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(243, isr_stub_243, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(244, isr_stub_244, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(245, isr_stub_245, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(246, isr_stub_246, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(247, isr_stub_247, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(248, isr_stub_248, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(249, isr_stub_249, IST0_DISABLED, TYPE_INTE);

	idt_set_gate(250, isr_stub_250, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(251, isr_stub_251, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(252, isr_stub_252, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(253, isr_stub_253, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(254, isr_stub_254, IST0_DISABLED, TYPE_INTE);
	idt_set_gate(255, isr_stub_255, IST0_DISABLED, TYPE_INTE);
}