/*--------------------------------------------------------------------------
*  File name:  vesa.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 21-11-2022
*--------------------------------------------------------------------------
Rotinas para manipulação de vídeo svga
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "io.h"
#include "ktypes.h"
#include "vesa.h"
#include "smp.h"
#include "mm/fixmap.h"
#include "console.h"
#include "lapic.h"

static struct
{
   struct vbe_info_block *info;
   struct vbe_mode_info_block *mode;
} vesa_data;

// Estruturas com informações sobre o modo de vídeo
vbe_info_t vbe_hinfo;
mode_info_t vbe_hmode;

/* Varávies públicas com os dados básicos para o
gerenciamento do modo de vídeo. */
uint32_t video_physbase;
uint16_t video_xbytes;
uint16_t video_xres;
uint16_t video_yres;
uint8_t video_bpp;
uint8_t *video_buffer;

u64_t mm_video_pgprot = (PG_FLAG_P | PG_FLAG_W | PG_FLAG_NC | PG_FLAG_U);
// u64_t mm_video_pgprot = (PG_FLAG_P | PG_FLAG_W | PG_FLAG_NC);

/**
 * @brief Extrai a configuração do modo de vídeo gravada pela rotina smp_ini(),
 * utilizada como trampolin para a ativação de um dos núcleos do smp. Ela pres-
 * supõe que a rotina smp_ini() já tenha sido invocada e modificado o modo de
 * vídeo, momento em que grava as informações na sua estrutura provisória.
 * Aqui nós fazemos a cópia daquela dado provisório para a estrutura definiti-
 * va "vbe_mode". Depois atribuímos os valores mais significativo parra as va-
 * riáveis públicas utilizadas para configurar o vídeo.
 *
 * @param base_ap
 */
void get_vbe_mode_info(u32_t trampoline_start)
{
   virt_addr_t source_mode = _get_vbe_mode_info();
   memcpy(&vbe_hmode, source_mode, sizeof(mode_info_t));

   video_xres = vbe_hmode.resolutionX;
   video_yres = vbe_hmode.resolutionY;
   video_xbytes = vbe_hmode.pitch;
   video_bpp = vbe_hmode.bpp;
   video_physbase = vbe_hmode.physbase;
   video_buffer = (virt_addr_t)get_virt_video_graphic();
}
void show_video_atribs(void)
{
   get_vbe_mode_info(TRAMPOLINE_START);
   kprintf("\nvideo_xres=%d", video_xres);
   kprintf("\nvideo_yres=%d", video_yres);
   kprintf("\nvideo_video_xbytes=%d", video_xbytes);
   kprintf("\nvideo_bpp=%d", video_bpp);
   kprintf("\nvideo_physbase=%x", video_physbase);
   kprintf("\nvideo_buffer=%p", video_buffer);
   kprintf("\n_get_video_status=%x", _get_video_status());

   if (is_video_ini_ok())
      kprintf("\nVideo ini OK");
}
/**
 * @brief Rotina que extrai as informações de configuração do modo de
 * vídeo e depois faz o mapeamento da memória necessária.
 *
 */
void map_video_mem(void)
{
   get_vbe_mode_info(TRAMPOLINE_START);

   virt_addr_t video_offset = (virt_addr_t)get_virt_video_graphic();
   u32_t video_size = align_up(video_xres * video_yres * (video_bpp / 8), PAGE_SIZE);
   phys_addr_t video_physend = video_physbase + video_size;

   /* Mapeamento da memória de vídeo que será utilizada. */
   for (u64_t i = video_physbase; i < video_physend; i += PAGE_SIZE)
   {
      kmap_frame(video_offset, i, mm_video_pgprot);
      video_offset = incptr(video_offset, PAGE_SIZE);
   }
   memset(video_buffer, 0, video_size);
}
/**
 * @brief Faz a inicialização do graphic mode, começando com a
 * inicialização de um dos cores e execução das rotinas VESA
 * no modo real. Toda a sequência foi concentrada nesta rotina.
 *
 */
void setup_vesa(void)
{
   /* Faz uma execução do trampoline. O trampoline inicializa um dos cores
   no mode real e faz a chamada de BIOS que modificada o modo de vídeo.
   */
   vesa_init();

   /*
Laço que testa a correta inicialização do vídeo. Sem este teste,
o sistema fica instável, pois o mapeamento da memória e a deleção
do seu conteúdo, antes de concluída a configuração e inicialziação,
no trapoline, gera muitos erros. Este precisa ser melhorado.
*/
   for (size_t i = 0; i < 10; i++)
   {
      if (is_video_ini_ok())
      {
         break;
      }
      hpet_sleep_milli(500);
   }
   /* Testamos para verificar se a inicialização do vídeo foi realizada
   corretamente. Caso contrário, permanecemos no modo texto. Essa veri-
   ficação foi necessária para eliminar vários erros.
   */
   if (!is_video_ini_ok())
   {
      return;
   }

   /*
   Fixa o modo de vídeo atual como gráfico e segue na fase seguinte, de
   mapeamento de memória criação das estrutura gráficas.
   */
   set_graphic_mode(true);
   if (is_graphic_mode())
   {
      /* Extrai as informações do vesa_trampoline e mapeia a memória
       de vídeo necessária para o funcionamento do modo gráfico.
       */
      map_video_mem();

      /* Cria as estruturas para gerenciamento e operação do modo gráfico. */
      graphic_obj = graphics_create_root();
      console_obj = console_init(graphic_obj);
      console_addref(console_obj);
   }
   console_clear_screen(console_obj);
}

static void vesa_boot_ap(size_t index)
{
   lapic_core_t *lapic = &apic_system.lapic_list[index];

   /* Send the INIT IPI */
   send_apic_ipi(lapic->id, IPI_INIT);
   hpet_sleep_milli(10);

   /**
    * Aqui eviamos uma mensagem para determinado core(dest_id) com o "Delivery Mode"
    * 110(Start-Up). Enviamos também um Vetor(índice) que aponta o page frame, onde
    * inicia o código a ser executado. Como se trata de uma interrupção utilizada na
    * inicialização dos cores, a execução é em real modo e deve atuar diretamente na
    * memória física, com limitações de endereçamento em 1M. O vetor 1 aponta para o
    * endereço 0x1000, o 2 para 0x2000 e assim por diante.
    *
    */

   send_apic_ipi(lapic->id, IPI_START_UP | ((uint32_t)TRAMPOLINE_START / PAGE_SIZE));
   hpet_sleep_milli(10);
   /*
      if (!smp_ap_started_flag)
      // if (false)
      {
         // Send SIPI again (second attempt)
         send_apic_ipi(lapic->id, IPI_START_UP | ((uint32_t)TRAMPOLINE_START / PAGE_SIZE));

         hpet_sleep_milli_sec(10);

         if (!smp_ap_started_flag)
         {
            kprintf("\nCPU %d failed to boot\n", index);
            lapic->present = 0;
            return;
         }
      }
      */
}

void vesa_init(void)
{
   kprintf("\nVESA - Ativando MODO GRAFICO.");

   // Localização virtual do trampoline no código e o seu tamanho
   virt_addr_t ini_code = (virt_addr_t)&vesa_realmode_start;
   virt_addr_t pend_code = (virt_addr_t)&vesa_realmode_end;
   u32_t code_size = subptrs((pend_code), (ini_code));

   // Local na memória física a ser copiado o trampoline.
   phys_addr_t mm_base = TRAMPOLINE_START;
   phys_addr_t pend = round_up(mm_base + code_size, PAGE_SIZE);

   for (size_t i = mm_base; i < pend; i += PAGE_SIZE)
   {
      set_page_allocated(i);
      kmap_frame((virt_addr_t)i, i, mm_video_pgprot);
   }
   memset((virt_addr_t)mm_base, 0, code_size);
   memcpy((virt_addr_t)mm_base, (virt_addr_t)ini_code, code_size);

   u8_t bsd_id = cpu_id();
   kprintf("\nnext AP=%d", bsd_id + 1);
   vesa_boot_ap(bsd_id + 1);

   kprintf("\nFinished AP boot sequence");
}
/* Rotina apenas para teste. Deve ser excluída. */
void testa_map_video_mem(void)
{
   video_physbase = 0xFD000000;
   virt_addr_t video_offset = (virt_addr_t)get_virt_video_graphic();
   video_buffer = video_offset;
   u32_t video_size = 800 * 600 * 3;
   phys_addr_t video_physend = video_physbase + video_size;

   kprintf("\nvideo_physbase=%x", video_physbase);
   kprintf("\nfixBase=%p", video_offset);
   kprintf("\nvideo_mem_size=%x", video_size);
   kprintf("\nvideo_physend=%x", video_physend);

   for (u64_t i = video_physbase; i < video_physend; i += PAGE_SIZE)
   {
      kmap_frame(video_offset, i, mm_video_pgprot);
      video_offset = (virt_addr_t)incptr(video_offset, PAGE_SIZE);
   }
   kprintf("\nfixBase=%p", video_offset);
}
