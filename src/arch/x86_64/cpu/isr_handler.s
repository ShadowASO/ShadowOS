;/*--------------------------------------------------------------------------
;*  File name:  isr_handler.s
;*  Author:  Aldenor Sombra de Oliveira
;*  Data de criação: 01-08-2020
;*--------------------------------------------------------------------------
;Este arquivo possui as rotinas para a manipulação de interrupções 
;--------------------------------------------------------------------------*/
; Interrupt descriptor table

%include "sys.inc"
%include "x86_64.inc"

section .data
align 16
;global idt64, pointer ;idt64.pointer

global isr_common
isr_common:	
	
	cli	
	PUSH_ALL
	
	mov rax, rsp ; Salvo o topo da stack em RAX - apenas um teste.
	mov rdi, rsp	;Pasagem do RSP como parâmetro para a função	
	
	extern isr_global_handler
	call isr_global_handler
	    	
	POP_ALL			
	add rsp, 16 ; erroNum e intNum		
	sti	
	
	iretq	

;********************************************************************
; Funções handlers
;********************************************************************
extern isr_save_context
[global isr_stub_0]
isr_stub_0:
 	push qword 0
	push qword 0
 jmp isr_common
 

[global isr_stub_1]
isr_stub_1:
	push qword 0
	push qword 1
 jmp isr_common
 

[global isr_stub_2]
isr_stub_2:
	push qword 0
	push qword 2
 jmp isr_common 

[global isr_stub_3]
isr_stub_3: 
 	push qword 0
	push qword 3
 jmp isr_common
 

[global isr_stub_4]
isr_stub_4:
	push qword 0
	push qword 4
 jmp isr_common
 

[global isr_stub_5]
isr_stub_5:
	push qword 0
	push qword 5
 jmp isr_common
 

[global isr_stub_6]
isr_stub_6:
	push qword 0
	push qword 6
 jmp isr_common
 
[global isr_stub_7]
isr_stub_7:
	push qword 0
	push qword 7
 jmp isr_common
 
[global isr_stub_8]
isr_stub_8:
	push qword 8
 jmp isr_common
 
[global isr_stub_9]
isr_stub_9:
	push dword 0
	push qword 9
 jmp isr_common
 
[global isr_stub_10]
isr_stub_10:
	push qword 10
 jmp isr_common
 
[global isr_stub_11]
isr_stub_11:
	push qword 11
 jmp isr_common
 
[global isr_stub_12]
isr_stub_12:
	push qword 12
 jmp isr_common
 
[global isr_stub_13]
isr_stub_13: 
	push qword 13
 jmp isr_common
 
[global isr_stub_14]
isr_stub_14:
	push qword 14
 jmp isr_common
 
[global isr_stub_15]
isr_stub_15:
	push qword 0
	push qword 15
 jmp isr_common
 
[global isr_stub_16]
isr_stub_16: 
 	push dword 0
	push qword 16
 jmp isr_common
 
[global isr_stub_17]
isr_stub_17:
 	push qword 17
 jmp isr_common
 
[global isr_stub_18]
isr_stub_18:
 	push qword 0
	push qword 18
 jmp isr_common
 
[global isr_stub_19]
isr_stub_19:
	push qword 0
	push qword 19
 jmp isr_common
 
[global isr_stub_20]
isr_stub_20:
	push qword 0
	push qword 20
 jmp isr_common

[global isr_stub_21]
isr_stub_21:
	push qword 0
	push qword 21
 jmp isr_common

[global isr_stub_22]
isr_stub_22:
	push qword 0
	push qword 22
 jmp isr_common

[global isr_stub_23]
isr_stub_23:
 	push qword 0
	push qword 23
 jmp isr_common

[global isr_stub_24]
isr_stub_24: 
	push qword 0
	push qword 24
 jmp isr_common 

[global isr_stub_25]
isr_stub_25:
	push qword 0
	push qword 25
 jmp isr_common

[global isr_stub_26]
isr_stub_26:
	push qword 0
	push qword 26
 jmp isr_common

[global isr_stub_27]
isr_stub_27:
	push qword 0
	push qword 27
 jmp isr_common

[global isr_stub_28]
isr_stub_28:
	push qword 0
	push qword 28
 jmp isr_common

[global isr_stub_29]
isr_stub_29:
	push qword 0
	push qword 29
 jmp isr_common

[global isr_stub_30]
isr_stub_30:	
	push qword 30
 jmp isr_common

[global isr_stub_31]
isr_stub_31:
	push qword 0
	push qword 31
 jmp isr_common

[global isr_stub_32]
isr_stub_32:
 push qword 0
 push qword 32 
jmp isr_common

 

[global isr_stub_33]
isr_stub_33:
	push qword 0
	push qword 33	
 jmp isr_common

[global isr_stub_34]
isr_stub_34:
	push qword 0
	push qword 34
 jmp isr_common 

[global isr_stub_35]
isr_stub_35:
	push qword 0
	push qword 35
 jmp isr_common 

[global isr_stub_36]
isr_stub_36:
	push qword 0
	push qword 36
 jmp isr_common 

[global isr_stub_37]
isr_stub_37:
	push qword 0
	push qword 37
 jmp isr_common 

[global isr_stub_38]
isr_stub_38:
	push qword 0
	push qword 38
 jmp isr_common 

[global isr_stub_39]
isr_stub_39:
	push qword 0
	push qword 39
 jmp isr_common 

[global isr_stub_40]
isr_stub_40:
	push qword 0
	push qword 40
 jmp isr_common 
 

[global isr_stub_41]
isr_stub_41:
	push qword 0
	push qword 41
 jmp isr_common 

[global isr_stub_42]
isr_stub_42:
	push qword 0
	push qword 42
 jmp isr_common 

[global isr_stub_43]
isr_stub_43:
	push qword 0
	push qword 43
 jmp isr_common 

[global isr_stub_44]
isr_stub_44:
	push qword 0
	push qword 44
 jmp isr_common 

[global isr_stub_45]
isr_stub_45:
	push qword 0
	push qword 45
 jmp isr_common 

[global isr_stub_46]
isr_stub_46:
	push qword 0
	push qword 46
 jmp isr_common 

[global isr_stub_47]
isr_stub_47:
	push qword 0
	push qword 47
 jmp isr_common 


[global isr_stub_48]
isr_stub_48:
	push qword 0
 	push qword 48
 jmp isr_common
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

[global isr_stub_49]
isr_stub_49:
push qword 0
 push qword 49
 jmp isr_common

[global isr_stub_50]
isr_stub_50:
push qword 0
 push qword 50
 jmp isr_common

[global isr_stub_51]
isr_stub_51:
push qword 0
 push qword 51
 jmp isr_common


[global isr_stub_52]
isr_stub_52:
push qword 0
 push qword 52
 jmp isr_common

[global isr_stub_53]
isr_stub_53:
push qword 0
 push qword 53
 jmp isr_common

[global isr_stub_54]
isr_stub_54:
push qword 0
 push qword 54
 jmp isr_common

[global isr_stub_55]
isr_stub_55:
push qword 0
 push qword 55
 jmp isr_common

[global isr_stub_56]
isr_stub_56:
push qword 0
 push qword 56
 jmp isr_common

[global isr_stub_57]
isr_stub_57:
push qword 0
 push qword 57
 jmp isr_common

[global isr_stub_58]
isr_stub_58:
push qword 0
 push qword 58
 jmp isr_common

[global isr_stub_59]
isr_stub_59:
push qword 0
 push qword 59
 jmp isr_common

[global isr_stub_60]
isr_stub_60:
push qword 0
 push qword 60
jmp isr_common
;jmp __switch_context

[global isr_stub_61]

isr_stub_61:
push qword 0
 push qword 61
 jmp isr_common

[global isr_stub_62]
isr_stub_62:
push qword 0
 push qword 62
 jmp isr_common

[global isr_stub_63]
isr_stub_63:
push qword 0
 push qword 63
 jmp isr_common

[global isr_stub_64]
isr_stub_64:
push qword 0
 push qword 64
 jmp isr_common

[global isr_stub_65]
isr_stub_65:
push qword 0
 push qword 65
 jmp isr_common

[global isr_stub_66]
isr_stub_66:
push qword 0
 push qword 66
 jmp isr_common

[global isr_stub_67]
isr_stub_67:
push qword 0
 push qword 67
 jmp isr_common

[global isr_stub_68]
isr_stub_68:
push qword 0
 push qword 68
 jmp isr_common

[global isr_stub_69]
isr_stub_69:
push qword 0
 push qword 69
 jmp isr_common

[global isr_stub_70]
isr_stub_70:
push qword 0
 push qword 70
;jmp isr_save_context
jmp isr_common

[global isr_stub_71]
isr_stub_71:
push qword 0
 push qword 71
 jmp isr_common

[global isr_stub_72]
isr_stub_72:
push qword 0
 push qword 72
 jmp isr_common

[global isr_stub_73]
isr_stub_73:
push qword 0
 push qword 73
 jmp isr_common

[global isr_stub_74]
isr_stub_74:
push qword 0
 push qword 74
 jmp isr_common

[global isr_stub_75]
isr_stub_75:
push qword 0
 push qword 75
 jmp isr_common

[global isr_stub_76]

isr_stub_76:
push qword 0
 push qword 76
 jmp isr_common

[global isr_stub_77]
isr_stub_77:
push qword 0
 push qword 77
 jmp isr_common

[global isr_stub_78]
isr_stub_78:
push qword 0
 push qword 78
 jmp isr_common

[global isr_stub_79]
isr_stub_79:
push qword 0
 push qword 79
 jmp isr_common

[global isr_stub_80]
isr_stub_80:
push qword 0
 push qword 80
 jmp isr_common

[global isr_stub_81]
isr_stub_81:
push qword 0
 push qword 81
 jmp isr_common

[global isr_stub_82]
isr_stub_82:
push qword 0
 push qword 82
 jmp isr_common

[global isr_stub_83]
isr_stub_83:
push qword 0
 push qword 83
 jmp isr_common

[global isr_stub_84]
isr_stub_84:
push qword 0
 push qword 84
 jmp isr_common

[global isr_stub_85]
isr_stub_85:
push qword 0
 push qword 85
 jmp isr_common

[global isr_stub_86]
isr_stub_86:
push qword 0
 push qword 86
 jmp isr_common

[global isr_stub_87]
isr_stub_87:
push qword 0
 push qword 87
 jmp isr_common

[global isr_stub_88]
isr_stub_88:
push qword 0
 push qword 88
 jmp isr_common

[global isr_stub_89]
isr_stub_89:
push qword 0
 push qword 89
 jmp isr_common

[global isr_stub_90]
isr_stub_90:
push qword 0
 push qword 90
 jmp isr_common

[global isr_stub_91]
isr_stub_91:
push qword 0
 push qword 91
 jmp isr_common

[global isr_stub_92]
isr_stub_92:
push qword 0
 push qword 92
 jmp isr_common

[global isr_stub_93]
isr_stub_93:
push qword 0
 push qword 93
 jmp isr_common

[global isr_stub_94]
isr_stub_94:
push qword 0
 push qword 94
 jmp isr_common

[global isr_stub_95]
isr_stub_95:
push qword 0
 push qword 95
 jmp isr_common

[global isr_stub_96]
isr_stub_96:
push qword 0
 push qword 96
 jmp isr_common

[global isr_stub_97]
isr_stub_97:
push qword 0
 push qword 97
 jmp isr_common

[global isr_stub_98]
isr_stub_98:
push qword 0
 push qword 98
 jmp isr_common

[global isr_stub_99]
isr_stub_99:
push qword 0
 push qword 99
 jmp isr_common

[global isr_stub_100]
isr_stub_100:
push qword 0
 push qword 100
 jmp isr_common

[global isr_stub_101]
isr_stub_101:
 push qword 101
 jmp isr_common

[global isr_stub_102]
isr_stub_102:
 push qword 102
 jmp isr_common

[global isr_stub_103]
isr_stub_103:
 push qword 103
 jmp isr_common

[global isr_stub_104]
isr_stub_104:
 push qword 104
 jmp isr_common

[global isr_stub_105]
isr_stub_105:
 push qword 105
 jmp isr_common

[global isr_stub_106]
isr_stub_106:
 push qword 106
 jmp isr_common

[global isr_stub_107]
isr_stub_107:
 push qword 107
 jmp isr_common

[global isr_stub_108]
isr_stub_108:
 push qword 108
 jmp isr_common

[global isr_stub_109]
isr_stub_109:
 push qword 109
 jmp isr_common

[global isr_stub_110]
isr_stub_110:
 push qword 110
 jmp isr_common

[global isr_stub_111]
isr_stub_111:
 push qword 111
 jmp isr_common

[global isr_stub_112]
isr_stub_112:
 push qword 112
 jmp isr_common

[global isr_stub_113]
isr_stub_113:
 push qword 113
 jmp isr_common

[global isr_stub_114]
isr_stub_114:
 push qword 114
 jmp isr_common

[global isr_stub_115]
isr_stub_115:
 push qword 115
 jmp isr_common

[global isr_stub_116]
isr_stub_116:
 push qword 116
 jmp isr_common

[global isr_stub_117]
isr_stub_117:
 push qword 117
 jmp isr_common

[global isr_stub_118]
isr_stub_118:
 push qword 118
 jmp isr_common

[global isr_stub_119]
isr_stub_119:
 push qword 119
 jmp isr_common

[global isr_stub_120]
isr_stub_120:
 push qword 120
 jmp isr_common

[global isr_stub_121]
isr_stub_121:
 push qword 121
 jmp isr_common

[global isr_stub_122]
isr_stub_122:
 push qword 122
 jmp isr_common

[global isr_stub_123]
isr_stub_123:
 push qword 123
 jmp isr_common

[global isr_stub_124]
isr_stub_124:
 push qword 124
 jmp isr_common

[global isr_stub_125]
isr_stub_125:
 push qword 125
 jmp isr_common

[global isr_stub_126]
isr_stub_126:
 ;push qword 126
 jmp isr_common

[global isr_stub_127]
isr_stub_127:
 push qword 127
 jmp isr_common

[global isr_stub_128]
isr_stub_128:
	push qword 0
	push qword 128
jmp isr_common


[global isr_stub_129]
isr_stub_129:
 push qword 129
 jmp isr_common

[global isr_stub_130]
isr_stub_130:
 push qword 130
 jmp isr_common

[global isr_stub_131]
isr_stub_131:
 push qword 131
 jmp isr_common

[global isr_stub_132]
isr_stub_132:
 push qword 132
 jmp isr_common

[global isr_stub_133]
isr_stub_133:
 push qword 133
 jmp isr_common

[global isr_stub_134]
isr_stub_134:
 push qword 134
 jmp isr_common

[global isr_stub_135]
isr_stub_135:
 push qword 135
 jmp isr_common

[global isr_stub_136]
isr_stub_136:
 push qword 136
 jmp isr_common

[global isr_stub_137]
isr_stub_137:
 push qword 137
 jmp isr_common

[global isr_stub_138]
isr_stub_138:
 push qword 138
 jmp isr_common

[global isr_stub_139]
isr_stub_139:
 push qword 139
 jmp isr_common

[global isr_stub_140]
isr_stub_140:
 push qword 140
 jmp isr_common

[global isr_stub_141]
isr_stub_141:
 push qword 141
 jmp isr_common

[global isr_stub_142]
isr_stub_142:
 push qword 142
 jmp isr_common

[global isr_stub_143]
isr_stub_143:
 push qword 143
 jmp isr_common

[global isr_stub_144]
isr_stub_144:
 push qword 144
 jmp isr_common

[global isr_stub_145]
isr_stub_145:
 push qword 145
 jmp isr_common

[global isr_stub_146]
isr_stub_146:
 push qword 146
 jmp isr_common

[global isr_stub_147]
isr_stub_147:
 push qword 147
 jmp isr_common

[global isr_stub_148]
isr_stub_148:
 push qword 148
 jmp isr_common

[global isr_stub_149]
isr_stub_149:
 push qword 149
 jmp isr_common

[global isr_stub_150]
isr_stub_150:
 push qword 150
 jmp isr_common

[global isr_stub_151]
isr_stub_151:
 push qword 151
 jmp isr_common

[global isr_stub_152]
isr_stub_152:
 push qword 152
 jmp isr_common

[global isr_stub_153]
isr_stub_153:
 push qword 153
 jmp isr_common

[global isr_stub_154]
isr_stub_154:
 push qword 154
 jmp isr_common

[global isr_stub_155]
isr_stub_155:
 push qword 155
 jmp isr_common

[global isr_stub_156]
isr_stub_156:
 push qword 156
 jmp isr_common

[global isr_stub_157]
isr_stub_157:
 push qword 157
 jmp isr_common

[global isr_stub_158]
isr_stub_158:
 push qword 158
 jmp isr_common

[global isr_stub_159]
isr_stub_159:
 push qword 159
 jmp isr_common

[global isr_stub_160]
isr_stub_160:
 push qword 160
 jmp isr_common

[global isr_stub_161]
isr_stub_161:
 push qword 161
 jmp isr_common

[global isr_stub_162]
isr_stub_162:
 push qword 162
 jmp isr_common

[global isr_stub_163]
isr_stub_163:
 push qword 163
 jmp isr_common

[global isr_stub_164]
isr_stub_164:
 push qword 164
 jmp isr_common

[global isr_stub_165]
isr_stub_165:
 push qword 165
 jmp isr_common
 
[global isr_stub_166]
isr_stub_166:
 push qword 166
 jmp isr_common

[global isr_stub_167]
isr_stub_167:
 push qword 167
 jmp isr_common

[global isr_stub_168]
isr_stub_168:
 push qword 168
 jmp isr_common

[global isr_stub_169]
isr_stub_169:
 push qword 169
 jmp isr_common

[global isr_stub_170]
isr_stub_170:
 push qword 170
 jmp isr_common

[global isr_stub_171]
isr_stub_171:
 push qword 171
 jmp isr_common

[global isr_stub_172]
isr_stub_172:
 push qword 172
 jmp isr_common

[global isr_stub_173]
isr_stub_173:
 push qword 173
 jmp isr_common

[global isr_stub_174]
isr_stub_174:
 push qword 174
 jmp isr_common

[global isr_stub_175]
isr_stub_175:
 push qword 175
 jmp isr_common

[global isr_stub_176]
isr_stub_176:
 push qword 176
 jmp isr_common

[global isr_stub_177]
isr_stub_177:
 push qword 177
 jmp isr_common

[global isr_stub_178]
isr_stub_178:
 push qword 178
 jmp isr_common

[global isr_stub_179]
isr_stub_179:
 push qword 179
 jmp isr_common

[global isr_stub_180]
isr_stub_180:
 push qword 180
 jmp isr_common

[global isr_stub_181]
isr_stub_181:
 push qword 181
 jmp isr_common

[global isr_stub_182]
isr_stub_182:
 push qword 182
 jmp isr_common

[global isr_stub_183]
isr_stub_183:
 push qword 183
 jmp isr_common

[global isr_stub_184]
isr_stub_184:
 push qword 184
 jmp isr_common

[global isr_stub_185]
isr_stub_185:
 push qword 185
 jmp isr_common

[global isr_stub_186]
isr_stub_186:
 push qword 186
 jmp isr_common

[global isr_stub_187]
isr_stub_187:
 push qword 187
 jmp isr_common

[global isr_stub_188]
isr_stub_188:
 push qword 188
 jmp isr_common

[global isr_stub_189]
isr_stub_189:
 push qword 189
 jmp isr_common

[global isr_stub_190]
isr_stub_190:
 push qword 190
 jmp isr_common

[global isr_stub_191]
isr_stub_191:
 push qword 191
 jmp isr_common

[global isr_stub_192]
isr_stub_192:
 push qword 192
 jmp isr_common

[global isr_stub_193]
isr_stub_193:
 push qword 193
 jmp isr_common

[global isr_stub_194]
isr_stub_194:
 push qword 194
 jmp isr_common

[global isr_stub_195]
isr_stub_195:
 push qword 195
 jmp isr_common

[global isr_stub_196]
isr_stub_196:
 push qword 196
 jmp isr_common

[global isr_stub_197]
isr_stub_197:
 push qword 197
 jmp isr_common

[global isr_stub_198]
isr_stub_198:
 push qword 198
 jmp isr_common

[global isr_stub_199]
isr_stub_199:
 push qword 199
 jmp isr_common

[global isr_stub_200]
isr_stub_200:
 push qword 200
 jmp isr_common

[global isr_stub_201]
isr_stub_201:
 push qword 201
 jmp isr_common

[global isr_stub_202]
isr_stub_202:
 push qword 202
 jmp isr_common

[global isr_stub_203]
isr_stub_203:
 push qword 203
 jmp isr_common

[global isr_stub_204]
isr_stub_204:
 push qword 204
 jmp isr_common

[global isr_stub_205]
isr_stub_205:
 push qword 205
 jmp isr_common

[global isr_stub_206]
isr_stub_206:
 push qword 206
 jmp isr_common

[global isr_stub_207]
isr_stub_207:
 push qword 207
 jmp isr_common

[global isr_stub_208]
isr_stub_208:
 push qword 208
 jmp isr_common

[global isr_stub_209]
isr_stub_209:
 push qword 209
 jmp isr_common

[global isr_stub_210]
isr_stub_210:
 push qword 210
 jmp isr_common

[global isr_stub_211]
isr_stub_211:
 push qword 211
 jmp isr_common

[global isr_stub_212]
isr_stub_212:
 push qword 212
 jmp isr_common

[global isr_stub_213]
isr_stub_213:
 push qword 213
 jmp isr_common

[global isr_stub_214]
isr_stub_214:
 push qword 214
 jmp isr_common

[global isr_stub_215]
isr_stub_215:
 push qword 215
 jmp isr_common

[global isr_stub_216]
isr_stub_216:
 push qword 216
 jmp isr_common

[global isr_stub_217]
isr_stub_217:
 push qword 217
 jmp isr_common

[global isr_stub_218]
isr_stub_218:
 push qword 218
 jmp isr_common

[global isr_stub_219]
isr_stub_219:
 push qword 219
 jmp isr_common

[global isr_stub_220]
isr_stub_220:
 push qword 220
 jmp isr_common

[global isr_stub_221]
isr_stub_221:
 push qword 221
 jmp isr_common

[global isr_stub_222]
isr_stub_222:
 push qword 222
 jmp isr_common

[global isr_stub_223]
isr_stub_223:
 push qword 223
 jmp isr_common

[global isr_stub_224]
isr_stub_224:
 push qword 224
 jmp isr_common

[global isr_stub_225]
isr_stub_225:
 push qword 225
 jmp isr_common

[global isr_stub_226]
isr_stub_226:
 push qword 226
 jmp isr_common

[global isr_stub_227]
isr_stub_227:
 push qword 227
 jmp isr_common

[global isr_stub_228]
isr_stub_228:
 push qword 228
 jmp isr_common

[global isr_stub_229]
isr_stub_229:
 push qword 229
 jmp isr_common

[global isr_stub_230]
isr_stub_230:
 push qword 230
 jmp isr_common

[global isr_stub_231]
isr_stub_231:
 push qword 231
 jmp isr_common

[global isr_stub_232]
isr_stub_232:
 push qword 232
 jmp isr_common

[global isr_stub_233]
isr_stub_233:
 push qword 233
 jmp isr_common

[global isr_stub_234]
isr_stub_234:
 push qword 234
 jmp isr_common

[global isr_stub_235]
isr_stub_235:
 push qword 235
 jmp isr_common

[global isr_stub_236]
isr_stub_236:
 push qword 236
 jmp isr_common

[global isr_stub_237]
isr_stub_237:
 push qword 237
 jmp isr_common

[global isr_stub_238]
isr_stub_238:
 push qword 238
 jmp isr_common

;--------------------------------------------------------
;Este handler é utilizado para as interrupções de tempo
;produzidas pelo APIC TIMER e utilizadas pelo scheduler.
;Passei a usar o IRQ_BASE==0xEF(239).
;--------------------------------------------------------
[global isr_stub_239]
isr_stub_239:
	push qword 0
 	push qword 239
 jmp isr_common
 ;--------------------------------------------------------

[global isr_stub_240]
isr_stub_240:
 push qword 240
 jmp isr_common

[global isr_stub_241]
isr_stub_241:
 push qword 241
 jmp isr_common

[global isr_stub_242]
isr_stub_242:
 push qword 242
 jmp isr_common

[global isr_stub_243]
isr_stub_243:
 push qword 243
 jmp isr_common

[global isr_stub_244]
isr_stub_244:
 push qword 244
 jmp isr_common

[global isr_stub_245]
isr_stub_245:
 push qword 245
 jmp isr_common

[global isr_stub_246]
isr_stub_246:
 push qword 246
 jmp isr_common

[global isr_stub_247]
isr_stub_247:
 push qword 247
 jmp isr_common

[global isr_stub_248]
isr_stub_248:
 push qword 248
 jmp isr_common

[global isr_stub_249]
isr_stub_249:
 push qword 249
 jmp isr_common

[global isr_stub_250]
isr_stub_250:
 push qword 250
 jmp isr_common

[global isr_stub_251]
isr_stub_251:
	push qword 0
	push qword 251 
 jmp isr_common

[global isr_stub_252]
isr_stub_252:
 push qword 252
 jmp isr_common

[global isr_stub_253]
isr_stub_253:
 push qword 253
 jmp isr_common

[global isr_stub_254]
isr_stub_254:
 push qword 0
	push qword 254 
 jmp isr_common

[global isr_stub_255]
isr_stub_255:
	push qword 0
	push qword 255 
 jmp isr_common