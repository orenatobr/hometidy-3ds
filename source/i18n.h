/** @file i18n.h
 *  UI strings in English and Brazilian Portuguese, chosen from the system language.
 */
#ifndef HT_I18N_H
#define HT_I18N_H

/* X(id, English, Portuguese) */
#define HT_STRINGS(X) \
    X(TAGLINE, "Sort the HOME Menu, check off finished games", "Ordene o HOME, marque jogos concluídos") \
    X(MENU_SORT, "Sort HOME Menu", "Ordenar o HOME") \
    X(MENU_SORT_SUB, "Alphabetical, folders last", "Alfabética, pastas por último") \
    X(MENU_MARK, "Green check on a game", "Selo verde em um jogo") \
    X(MENU_MARK_SUB, "Mark or unmark finished games", "Marque ou desmarque jogos concluídos") \
    X(HINT_MENU, "A  Select      START  Exit", "A  Escolher      START  Sair") \
    X(LOADING, "Reading HOME data...", "Lendo os dados do HOME...") \
    X(INFO_HOME, "%d titles in %d folders", "%d títulos em %d pastas") \
    X(INFO_NAMES, "%d names read from the icon cache", "%d nomes lidos do cache de ícones") \
    X(INFO_UNNAMED, "%d titles without a name stay in front", "%d títulos sem nome ficam na frente") \
    X(SORT_BLOCKED, "%d names could not be read, so sorting is off.", "%d nomes não puderam ser lidos, então a ordenação está desligada.") \
    X(SORT_TITLE, "Sort HOME Menu", "Ordenar o HOME") \
    X(SORT_CHANGES, "%d positions change", "%d posições mudam") \
    X(SORT_SORTED, "Already sorted", "Já está ordenado") \
    X(SORT_NOTE, "A backup goes to the SD card first.", "Antes, um backup vai para o cartão SD.") \
    X(BTN_APPLY, "Apply", "Aplicar") \
    X(BTN_BACK, "Back", "Voltar") \
    X(BTN_RESTORE, "Restore last backup", "Restaurar o último backup") \
    X(BTN_REBOOT, "Reboot now", "Reiniciar agora") \
    X(BTN_YES, "Yes", "Sim") \
    X(BTN_NO, "No", "Não") \
    X(CONFIRM_SORT, "Write the new order?", "Gravar a nova ordem?") \
    X(CONFIRM_RESTORE, "Restore the HOME layout from the last backup?", "Restaurar o HOME a partir do último backup?") \
    X(WORK_BACKUP, "Backing up...", "Fazendo backup...") \
    X(WORK_WRITE, "Writing...", "Gravando...") \
    X(WORK_VERIFY, "Checking...", "Conferindo...") \
    X(DONE_SORT, "New order saved", "Nova ordem gravada") \
    X(DONE_MARK, "Icons updated", "Ícones atualizados") \
    X(DONE_RESTORE, "Backup restored", "Backup restaurado") \
    X(DONE_BACKUP_AT, "Backup: %s", "Backup: %s") \
    X(DONE_NOHOME, "Do not press HOME: it would overwrite the change.", "Não aperte HOME: ele sobrescreveria a mudança.") \
    X(DONE_REBOOT, "Reboot now, or hold power and choose Reboot (Luma).", "Reinicie agora, ou segure power e escolha Reboot (Luma).") \
    X(DONE_LEAVE, "B  Leave without rebooting", "B  Sair sem reiniciar") \
    X(REBOOTING, "Rebooting...", "Reiniciando...") \
    X(REBOOT_FAIL, "Reboot failed (%08lX). Hold power and choose Reboot.", "Falha ao reiniciar (%08lX). Segure power e escolha Reboot.") \
    X(MARK_TITLE, "Green check", "Selo verde") \
    X(MARK_HINT, "A  Mark/unmark     B  Back     L/R  Page", "A  Marcar/desmarcar     B  Voltar     L/R  Página") \
    X(MARK_ON, "Marked", "Marcado") \
    X(MARK_OFF, "Not marked", "Sem selo") \
    X(MARK_ORIGINAL, "Original", "Original") \
    X(MARK_BEFORE, "Now", "Agora") \
    X(MARK_AFTER, "With check", "Com selo") \
    X(MARK_NO_BACKUP, "Marked by an older tool: no original to restore.", "Marcado por outra ferramenta: sem original para restaurar.") \
    X(MARK_EMPTY, "No games found in the icon cache.", "Nenhum jogo no cache de ícones.") \
    X(ERR_TITLE, "Something went wrong", "Algo deu errado") \
    X(ERR_OPEN, "Could not open the HOME Menu saves (%08lX).", "Não consegui abrir os saves do HOME (%08lX).") \
    X(ERR_OPEN_HINT, "Install the CIA with FBI on a Luma3DS console, EUR region.", "Instale o CIA com o FBI em um console com Luma3DS, região EUR.") \
    X(ERR_READ, "Could not read %s (%08lX).", "Não consegui ler %s (%08lX).") \
    X(ERR_FORMAT, "Unexpected HOME file layout (not an EUR HOME Menu?).", "Formato inesperado dos arquivos do HOME (não é um HOME EUR?).") \
    X(ERR_BACKUP, "Could not write the backup to the SD card.", "Não consegui gravar o backup no cartão SD.") \
    X(ERR_WRITE, "Write failed (%08lX). The original was put back.", "A gravação falhou (%08lX). O original foi recolocado.") \
    X(ERR_VERIFY, "Check failed: what was read back differs. The original was put back.", "A conferência falhou: o que foi lido difere. O original foi recolocado.") \
    X(ERR_NOBACKUP, "No backup found on the SD card.", "Nenhum backup no cartão SD.") \
    X(ERR_BAD_BACKUP, "The backup files look wrong. Nothing was written.", "Os arquivos do backup parecem errados. Nada foi gravado.") \
    X(ERR_MEMORY, "Out of memory.", "Sem memória.") \
    X(HINT_BACK, "A / B  Back", "A / B  Voltar")

typedef enum {
#define X(id, en, pt) S_##id,
    HT_STRINGS(X)
#undef X
    S_COUNT
} StrId;

/** Choose the language from the system settings (Portuguese, otherwise English). */
void i18n_init(void);

/** Text for an id in the current language. */
const char *tr(StrId id);

#endif
