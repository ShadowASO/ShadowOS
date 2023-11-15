/*As variáveis criadas neste arquivo aparecem para o sistema operacional como 
ponteiros "extern unint64_t _Variavel" e seu conteúdo deve ser acessado usando
 */

OUTPUT_FORMAT(elf64-x86-64)

/* Indica o entry point da aplicação. */
ENTRY(_start)

/* Tamanho de página usado para alinhamento. */
_PAGE_SIZE_ = 4K;

/* Endereço da RAM onde a imagem deve ser carregada=2M. */
_PHYS_OFFSET_ = 0x100000; 

/* Endereço virtual onde a RAM será referenciada. */
_VIRT_OFFSET_ =   0xFFFFFFFF80000000;

virt = _VIRT_OFFSET_ + _PHYS_OFFSET_;

SECTIONS
{
	/* Fixa o location counter igual ao endereço inicial da memória física. */	
	 . = _PHYS_OFFSET_ + _VIRT_OFFSET_ + SIZEOF_HEADERS;    
	
	
	/* Variáveis utilizadas no código do kernel. */            
        _kernel_ini_virt = . ;
        _kernel_ini_phys = _PHYS_OFFSET_;    
        
                               
        .rodata ALIGN(_PAGE_SIZE_) : AT(ADDR(.rodata) - _VIRT_OFFSET_)
        {        	
                _rodata_begin_phys = . - _VIRT_OFFSET_;
                
                *(.multiboot_header)
                *(.rodata)
                *(.rodata*)
                *(.eh_frame)
                
                . = ALIGN(_PAGE_SIZE_); 
                _rodata_end_phys = . - _VIRT_OFFSET_;                
        }

        .text ALIGN(_PAGE_SIZE_) : AT(ADDR(.text) - _VIRT_OFFSET_)
        {        	
                _text_begin_phys = . - _VIRT_OFFSET_;
                
                *(.text)
                //*(.text*)
		//*(.init)
		//*(.fini)
		//*(.ctors)
		//*(.dtors)
		
		. = ALIGN(_PAGE_SIZE_); 
                _text_end_phys = . - _VIRT_OFFSET_;                
        }

        .data ALIGN(_PAGE_SIZE_) : AT(ADDR(.data) - _VIRT_OFFSET_)
        {        	
                _data_begin_phys = . - _VIRT_OFFSET_;
                
                *(.data)

                //. = ALIGN(_PAGE_SIZE_); 
                //. += _PAGE_SIZE_;
                
                _data_end_phys = . - _VIRT_OFFSET_;               
        }
	
	
        .bss ALIGN(_PAGE_SIZE_) : AT(ADDR(.bss) - _VIRT_OFFSET_)
        {        	
                _bss_begin_phys = . - _VIRT_OFFSET_;
                
                *(.bss)
                *(COMMON)
                
                               
                /* Crio um identificador para a área do pagetable p4. Em seguida, avanço o ponteiro
                de memória em 4096 bytes. */  
                . += _PAGE_SIZE_ ;             
                _swapper_pt4_dir = ALIGN(_PAGE_SIZE_) - _VIRT_OFFSET_;
                . += _PAGE_SIZE_ * 2 ;
                
                _bss_end_phys = . - _VIRT_OFFSET_;         
        }        

        /DISCARD/ :
        {        	
                *(.comment)                
        }
        . = ALIGN(_PAGE_SIZE_);        
        . += _PAGE_SIZE_;
        
        _kernel_end_phys = . - _VIRT_OFFSET_;
        _kernel_end_virt = . ;
        _kernel_size = _kernel_end_phys - _kernel_ini_phys ;

}
