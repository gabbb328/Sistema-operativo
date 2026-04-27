# Sistema Operativo "MyOS" - Progetto Universitario

Questo repository contiene il codice sorgente di un sistema operativo minimale sviluppato come progetto accademico. Il sistema è in grado di avviarsi tramite GRUB, gestire la memoria, processare input da tastiera e mouse, e visualizzare una semplice interfaccia grafica con finestre.

## 🚀 Come Compilare ed Eseguire

Assicurati di avere installato `nasm`, `gcc` (con supporto cross-compilazione a 32-bit) e `grub-mkrescue`.

1. **Pulizia** (opzionale):
   ```bash
   make clean
   ```

2. **Compilazione e Creazione ISO**:
   ```bash
   make all
   ```

3. **Esecuzione su QEMU**:
   ```bash
   qemu-system-i386 -cdrom os.iso -m 128M
   ```

## 🛠️ Funzionalità Implementate

### 1. Bootloader e Gestione Hardware
- **GRUB Multiboot**: Avvio sicuro del kernel.
- **GDT (Global Descriptor Table)**: Configurazione della memoria protetta.
- **IDT (Interrupt Descriptor Table)**: Gestione delle eccezioni e interrupt.
- **PIC (Programmable Interrupt Controller)**: Gestione degli interrupt hardware.

### 2. Gestione della Memoria
- **Paging**: Implementazione di tabelle di pagine per la gestione della memoria virtuale.
- **Kernel Allocator (`kmalloc`)**: Allocatore di memoria dinamica per il kernel.

### 3. Interfacce Utente
- **Terminal VGA**: Modalità testo standard (80x25).
- **Framebuffer VGA**: Modalità grafica con supporto per mouse e finestre.
- **Tastiera**: Gestione degli interrupt da tastiera e buffer di input.
- **Mouse PS/2**: Rilevamento del movimento e click del mouse.

### 4. Applicazioni
- **Task Manager**: Finestra per visualizzare lo stato del sistema.
- **Notepad**: Applicazione di testo con supporto per input da tastiera e mouse.
- **Lockscreen**: Schermata di blocco con password (default: `admin`/`1234`).

## 📂 Struttura del Progetto

- `src/`: Codice sorgente del kernel.
  - `boot.s`: Codice di bootstrap iniziale.
  - `kernel.c`: Funzione principale (`main`) e gestione dell'interfaccia grafica.
  - `gdt.c`, `idt.c`, `pic.c`, `irq.c`, `isr.c`: Gestione hardware e interrupt.
  - `paging.c`: Gestione della memoria virtuale.
  - `keyboard.c`, `mouse.c`: Gestione periferiche di input.
  - `kmalloc.c`: Allocatore di memoria.
- `iso/`: File generati per l'immagine ISO.
- `Makefile`: Script di build automatico.

## 📝 Note di Sviluppo

- Il sistema è progettato per girare in emulazione (QEMU).
- La password di default per sbloccare la Lockscreen è `admin` / `1234`.
- Il codice include commenti dettagliati per spiegare le varie componenti hardware e software.