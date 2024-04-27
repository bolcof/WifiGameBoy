; Video Stream for WiFi Game Boy Cartridge

SECTION	"start",ROM0[$0100]
    nop
    jp	begin


 DB $CE,$ED,$66,$66,$CC,$0D,$00,$0B,$03,$73,$00,$83,$00,$0C,$00,$0D
 DB $00,$08,$11,$1F,$88,$89,$00,$0E,$DC,$CC,$6E,$E6,$DD,$DD,$D9,$99
 DB $BB,$BB,$67,$63,$6E,$0E,$EC,$CC,$DD,$DC,$99,$9F,$BB,$B9,$33,$3E

 DB "VIDEOSTREAM",0,0,0,0     ; Cart name - 15bytes
 DB $80                       ; $143 - Card supports both, DMG and CGB double speed mode
 DB 0,0                       ; $144 - Licensee code
 DB 0                         ; $146 - SGB Support indicator
 DB 0                         ; $147 - Cart type
 DB 0                         ; $148 - ROM Size
 DB 0                         ; $149 - RAM Size
 DB 1                         ; $14a - Destination code
 DB $33                       ; $14b - Old licensee code
 DB 0                         ; $14c - Mask ROM version
 DB 0                         ; $14d - Complement check
 DW 0                         ; $14e - Checksum


MACRO ReadTile

	;We will load everything from 0x7ffe, so we set this address to bc once as we do not even need the register for anything else
	ld	de, $7ffe		;3 cycles

	;Sleep a while with a little leeway... (Total: n*4+2-1, so for example ReadTile 6 has 25 cycles in this loop)
	ld	a, \1		    ;2 cycles
	.sleep\@:
		dec a				;1 cycle
	jr	nz, .sleep\@	;3 cycles (-1 if not taken)

	;First read to trigger interrupt on ESP8266
	ld	a, [de]			;2 cycles

	ld	c, $41 			; We need to check ff41 here, 2 cycles
	.wait\@:				; Wait for hblank and copy 16 bytes
		ldh	a, [c]
		and	a, %00000011
	jr	nz, .wait\@

	; Load 16 bytes = 1 tile
	REPT 16		
		ld	a, [de]
		ld	[hli], a
	ENDR

    ENDM


MACRO WaitForMode

	ld	c, $41 ;We need to check ff41 here
	.waitMode\@:
		ldh	a, [c]
		and	a, %00000011
		cp  a, \1
    jr	nz, .waitMode\@

	ENDM

MACRO IgnoreHBlank
    WaitForMode %00000000
    WaitForMode %00000010
	ENDM

MACRO CGBReadTile

	;We will load everything from 0x7ffe, so we set this address to bc once as we do not even need the register for anything else
	ld	de, $7ffe		;3 cycles

	;Sleep a while with a little leeway... (Total: n*4+2-1, so for example ReadTile 6 has 25 cycles in this loop)
	ld	a, \1		    ;2 cycles
	.sleep\@:
		dec a				;1 cycle
	jr	nz, .sleep\@	;3 cycles (-1 if not taken)

	;First read to trigger interrupt on ESP8266
	ld	a, [de]			;2 cycles

	ld	c, $41 			; We need to check ff41 here, 2 cycles
	.wait\@:				; Wait for hblank and copy 32 bytes
		ldh	a, [c]
		and	a, %00000011
	jr	nz, .wait\@

	; Load 32 bytes = 1 tile
	REPT 32		
		ld	a, [de]
		ld	[hli], a
	ENDR

    ENDM

MACRO ReadTileAttributes
	;We will load everything from 0x7ffe, so we set this address to bc once as we do not even need the register for anything else
	ld	de, $7ffe		;3 cycles

	;Sleep a while with a little leeway... (Total: n*4+2-1, so for example ReadTile 6 has 25 cycles in this loop)
	ld	a, \1		    ;2 cycles
	.sleep\@:
		dec a				;1 cycle
	jr	nz, .sleep\@	;3 cycles (-1 if not taken)

	;First read to trigger interrupt on ESP8266
	ld	a, [de]			;2 cycles

	ld	c, $41 			; We need to check ff41 here, 2 cycles
	.wait\@:				; Wait for hblank and copy 32 bytes
		ldh	a, [c]
		and	a, %00000011
	jr	nz, .wait\@

	; Load 20 bytes = 20 tile attributes
	REPT 20		
		ld	a, [de]
		ld	[hli], a
	ENDR

    WaitForMode %00000010

    ENDM

MACRO ReadRow
	ld	hl, \1
    ld  a, $01
    ldh [$4f], a ; VRAM Bank 1
    ReadTileAttributes 17

	ld	hl, \2
    ld  a, $00
    ldh [$4f], a ; VRAM Bank 0
    REPT 10
        CGBReadTile 17
    ENDR

    ENDM


MACRO ReadPalettes
	;We will load everything from 0x7ffe, so we set this address to bc once as we do not even need the register for anything else
	ld	de, $7ffe		;3 cycles

	;First read to trigger interrupt on ESP8266
	ld	a, [de]			;2 cycles

    ;Set Background palette index to 0 and auto increment
    ld  a, $80
    ldh [$ff68], a

    ;We will write to $ff69
	ld	hl, $ff69

    REPT 10
        nop
    ENDR

	; Load 64 byte
	REPT 64		
		ld	a, [de]
		ld	[hl], a
	ENDR

    ENDM


MACRO SendJoypadState
	ld	hl, $ff00
	ld	a, %00100000	; Check button keys
	ld [hl], a
	ld	a, [hl]
    REPT 8
	    ld	c, [hl]
        or c
    ENDR
	cpl
	and	%00001111
	swap	a
	ld	b, a

	ld	a, %00010000	; Check direction keys
	ld [hl], a
	ld	a, [hl]
    REPT 8
	    ld	c, [hl]
        or c
    ENDR
	cpl
	and %00001111
	or b

	ld	hl, $7fff		; Send to 0x7fff
	ld	[hl], a
    REPT \1             ; Wait for the ESP (this is a bit longer than necessary, but we have time and it has to align with the following timing checks or we do not exacly fit into hblank)
        nop
    ENDR
	ld	[hl], a         ; Send again
	ENDM

MACRO NotifyCGBtoESP
    ld	hl, $7fff		; Send to 0x7fff
    ld  a, $f0          ; This cannot be generated by SendJoypadState as it would mean all directions on the D-Pad at once
	ld	[hl], a
    REPT 10             ; Wait for the ESP (this is a bit longer than necessary, but we have time and it has to align with the following timing checks or we do not exacly fit into hblank)
        nop
    ENDR
	ld	[hl], a         ; Send again
ENDM


begin:
    ; no interrupts needed
	di

    ; Check if we are on a Game Boy Color
    ld  b, a ; Store state of register a for CGB detection after initialization

    ; BG tile palette.
    ; Experienced GB developmers may hate me for this as it is reverse to common use, but I find this much more intuitive for video streams with 00 being black and 11 being white.
	ld	a, %00011011	; Window palette colors
	ld	[$FF47], a

    ; Set scroll registers to zero
	ld	a, 0
	ld	[$FF42], a
	ld	[$FF43], a

    ; Turn off LCD to load tile map
    ld	a, %00010001	;LCD off, BG Tile Data 0x8000, BG ON
	ld	[$ff40], a

	; Load tile map, which simply starts at 0, overflows once somewhere near the middle. We will switch the tile set to 0x8800 halfway through rendering later, so the overflow will point to other tiles.
LINEADDR = $9800
	ld	a, 0
	REPT 18
    	ld	hl, LINEADDR
LINEADDR = LINEADDR + 32
        REPT 20
		    ld	[hli], a
		    inc	a
        ENDR
	ENDR

    ; Turn on LCD with tile data starting at 0x8000
    ld	a, %10010001	;LCD on, BG Tile Data 0x8000, BG ON
	ld	[$ff40], a

    ; The CGB and especially the Analogue Pocket starts a bit too fast for the ESP8266 to connect to WiFi first. So, let's wait six seconds (360 vblanks).
	ld	d, 180
startDelay:
	WaitForMode %00000001		;Wait for vblank
	WaitForMode %00000011		;Wait for end of vblank
	WaitForMode %00000001		;Wait for vblank
	WaitForMode %00000011		;Wait for end of vblank
	ld a, d
	dec d
    dec a
	jp  nz, startDelay

    ; Now let's see if we are on a CGB or a DMG
	ld	a, b
	cp  $11
	jp	nz, loop        ; DMG

    ; We are in CGB mode.

    NotifyCGBtoESP      ; Let the ESP know that we are on a CGB    

    ; CGB, enable double speed
    ld	a, $01
	ld	[$ff4d], a    
    stop
	jp	cgbloop         ; CGB (or GBA)


loop: ; DMG mode

    ;We just have enough time to load one tile in each HBLANK period. VBLANK might yield another 16 tiles, so we would make 144+16 = 160 tiles per LCD refresh
    ;Therefore, we need three complete LCD refreshs in any case to load all 360 tiles, so we can stick to HBLANK loading with 120 tiles per refresh.
	ld	hl, $8000	;First tile goes here

	ld	b, 3 ; Loop over three LCD refreshs
	lcdLoop:

		; Wait for VBLANK
		WaitForMode %00000001

		; Switch back to tileset starting at 0x8000
		ld	a, %10010001	;LCD on, BG Tile Data 0x8000, BG ON
		ld	[$ff40], a

		;Wait for end of vblank
		WaitForMode %00000011

		; Read 73 tiles
		REPT 73
		    ReadTile 6
		ENDR

		; About half-way through. Switch to tileset starting at 0x8800
		ld	a, %10000001	;LCD on, BG Tile Data 0x8800, BG ON
		ld	[$ff40], a

		; Read remaining 47 tiles for a total of 120
		ReadTile 5 ; 4 cycles shorter than usual to account for tileset switch
		REPT 46
		    ReadTile 6
		ENDR

		; End of loop over three LCD refreshes
		ld a, b
		dec b
		dec a
	jp  nz, lcdLoop

	; Send joypad state. This also triggers the buffer swap and next send cycle on the ESP
	SendJoypadState 8

jp	loop


cgbloop: ; CGB mode

    ;In double speed mode, we can double the number of tiles to load, so we can easily do it in two refreshes and achieve 30 fps *woohoo*
    ;However, we now also want to transfer eight tile palettes (64 byte total) and tile map attributes (18*20 = 360 byte)
    ; The tile palettes will be transferred during vblank. Since we spread the image across to refreshes, color mismatches are unavoidable, so we want to change the palette only gradually.
    ; The tile map attributes are a bit trickier as we want to keep them close to the corresponding tile data. Unfortunately, this will push the data for each tile to 17 bytes and requires switching VRAM bank and jumping to another address.
    ; So we do not want to mix them completely. Instead, we will transfer attribute data for 20 tiles in one hblank and then load data for these 20 tiles in 10 consecutive hblanks.
    ; This means that each row of tiles will take 11 hblanks. With 18 rows across two refreshes this means 99 hblanks for each refresh.
    ; With so much headroom and only two refreshes, it is also easy to avoid replacing tiles that are already half drawn:
    ;   - On the first refresh we skip the first 8 hblanks while the first row of tiles is being drawn.
    ;   - On the second refresh we start immediately and can finish the last tiles before the last row is being drawn.
    ; In fact, this means perfect vsync! The first refresh runs ahead of the new tile data and redraws the old image. The second refresh runs behind the new data and will only draw the new image.
    ; Therefore, the tile palettes should be set in the vblank between these two!

    ; The ESP needs to send data in the following order: 9x [20x tile attributes (1B), 20x tile data (16B)], 8x color palette (8B), 9x [20x tile attributes (1B), 20x tile data (16B)]

    ;;;; FIRST REFRESH ;;;;

	; Wait for VBLANK
	WaitForMode %00000001

	; Switch back to tileset starting at 0x8000
	ld	a, %10010001	;LCD on, BG Tile Data 0x8000, BG ON
	ld	[$ff40], a

	ld	hl, $8000	;First tile goes here

	;Wait for end of vblank
	WaitForMode %00000011

    ;Read 6 rows after burning eight hblanks
    REPT 8
        IgnoreHBlank
    ENDR
    ReadRow $9800, $8000
    ReadRow $9820, $8140
    ReadRow $9840, $8280
    ReadRow $9860, $83c0
    ReadRow $9880, $8500
    ReadRow $98a0, $8640

	; About half-way through. Switch to tileset starting at 0x8800
	ld	a, %10000001	;LCD on, BG Tile Data 0x8800, BG ON
	ld	[$ff40], a

    ReadRow $98c0, $8780
    ReadRow $98e0, $88c0
    ReadRow $9900, $8a00

    ;;;; SECOND REFRESH ;;;;

	; Wait for VBLANK
	WaitForMode %00000001

	; Switch back to tileset starting at 0x8000
	ld	a, %10010001	;LCD on, BG Tile Data 0x8000, BG ON
	ld	[$ff40], a

    ReadPalettes

	;Wait for end of vblank
	WaitForMode %00000010

    ;Read 7 rows immediately
    ReadRow $9920, $8b40
    ReadRow $9940, $8c80
    ReadRow $9960, $8dc0
    ReadRow $9980, $8f00
    ReadRow $99a0, $9040
    ReadRow $99c0, $9180
    ReadRow $99e0, $92c0

	; About half-way through. Switch to tileset starting at 0x8800
	ld	a, %10000001	;LCD on, BG Tile Data 0x8800, BG ON
	ld	[$ff40], a

    ; Read remaining 2 rows
    ReadRow $9a00, $9400
    ReadRow $9a20, $9540

	; Send joypad state. This also triggers the buffer swap and next send cycle on the ESP
    IgnoreHBlank ;Wait a moment to make sure the ESP won't miss it after the last transferred tile
	SendJoypadState 9

jp	cgbloop

