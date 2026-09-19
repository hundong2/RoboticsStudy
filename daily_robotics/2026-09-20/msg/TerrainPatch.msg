# MCU에서 한 주기 동안 취득한 8x8 고도 패치다.
# 배열 길이가 고정되어 런타임 메모리와 직렬화 크기의 상한을 계산할 수 있다.
builtin_interfaces/Time stamp
uint32 sequence
float32 resolution_m
uint8 width
uint8 height
float32[64] elevation_m
uint8[64] valid
