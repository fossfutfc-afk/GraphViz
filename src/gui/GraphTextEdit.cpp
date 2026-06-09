#include "GraphTextEdit.h"

#include <QKeyEvent>
#include <QTextBlock>
#include <QTextCursor>

void GraphTextEdit::keyPressEvent(QKeyEvent *event)
{
    // Qt 将 Shift+Tab 映射为 Key_Backtab
    if (event->key() == Qt::Key_Backtab ||
        (event->key() == Qt::Key_Tab &&
         (event->modifiers() & Qt::ShiftModifier))) {
        // Shift+Tab / Backtab: un-indent
        QTextCursor cursor = textCursor();
        int start = cursor.selectionStart();
        int end   = cursor.selectionEnd();
        bool hasSelection = cursor.hasSelection();

        cursor.beginEditBlock();

        if (hasSelection) {
            QTextCursor selCursor = cursor;
            selCursor.setPosition(start);
            int startBlock = selCursor.block().blockNumber();
            selCursor.setPosition(end);
            int endBlock = selCursor.block().blockNumber();
            if (selCursor.atBlockStart() && endBlock > startBlock)
                --endBlock;

            for (int bn = startBlock; bn <= endBlock; ++bn) {
                QTextBlock block = document()->findBlockByNumber(bn);
                QString text = block.text();
                int removeCount = 0;
                if (!text.isEmpty()) {
                    if (text[0] == '\t') {
                        removeCount = 1;
                    } else {
                        for (int i = 0; i < text.size() && i < 4; ++i) {
                            if (text[i] == ' ') ++removeCount;
                            else break;
                        }
                    }
                }
                if (removeCount > 0) {
                    QTextCursor blockCursor(block);
                    blockCursor.movePosition(QTextCursor::StartOfBlock);
                    blockCursor.movePosition(QTextCursor::Right,
                        QTextCursor::KeepAnchor, removeCount);
                    blockCursor.removeSelectedText();
                }
            }
        } else {
            QTextBlock block = cursor.block();
            QString text = block.text();
            int removeCount = 0;
            if (!text.isEmpty()) {
                if (text[0] == '\t') {
                    removeCount = 1;
                } else {
                    for (int i = 0; i < text.size() && i < 4; ++i) {
                        if (text[i] == ' ') ++removeCount;
                        else break;
                    }
                }
            }
            if (removeCount > 0) {
                QTextCursor blockCursor(block);
                blockCursor.movePosition(QTextCursor::StartOfBlock);
                blockCursor.movePosition(QTextCursor::Right,
                    QTextCursor::KeepAnchor, removeCount);
                blockCursor.removeSelectedText();
            }
        }

        cursor.endEditBlock();
        return;  // 消费事件，不再传递
    }

    QPlainTextEdit::keyPressEvent(event);
}
