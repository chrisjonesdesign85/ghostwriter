/***********************************************************************
 *
 * Copyright (C) 2020 wereturtle
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ***********************************************************************/

#include <QQueue>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QStack>

#include "cmark-gfm-core-extensions.h"
#include "MarkdownNode.h"

MarkdownNode::MarkdownNode() :
    type(Invalid),
    parent(NULL),
    prev(NULL),
    next(NULL),
    firstChild(NULL),
    lastChild(NULL),
    startLine(0),
    endLine(0),
    position(0),
    length(0),
    fenceChar('\0'),
    headingLevel(0),
    listStartNum(0)
{
    ;
}

MarkdownNode::MarkdownNode
(
    cmark_node* node
)
{
    MarkdownNode();

    if (NULL == node)
    {
        return;
    }

    setDataFrom(node);
}

MarkdownNode::~MarkdownNode()
{
    ;
}

void MarkdownNode::setDataFrom(cmark_node* node)
{
    // Copy data.
    this->type = getNodeType(node);
    this->position = cmark_node_get_start_column(node) - 1;
    this->length = cmark_node_get_end_column(node) - cmark_node_get_start_column(node) + 1;
    this->startLine = cmark_node_get_start_line(node);
    this->endLine = cmark_node_get_end_line(node);

    if (!isBlockType())
    {
        this->text = cmark_node_get_literal(node);
    }

    if (CodeBlock == this->type)
    {
        int len;
        int offset;
        char ch;

        bool fenced = cmark_node_get_fenced(node, &len, &offset, &ch);

        if (fenced)
        {
            this->fenceChar = ch;
        }
    }
    else if (Heading == this->type)
    {
        this->headingLevel = cmark_node_get_heading_level(node);
        this->text = cmark_node_get_string_content(node);
        this->text = this->text.simplified();
    }    
}

MarkdownNode* MarkdownNode::getParent() const
{
    return this->parent;
}

void MarkdownNode::appendChild(MarkdownNode* node)
{
    if (NULL != node)
    {
        node->parent = this;

        if (NULL == firstChild)
        {
            firstChild = node;
            lastChild = node;
            node->prev = NULL;
            node->next = NULL;
        }
        else
        {
            lastChild->next = node;
            node->prev = lastChild;
            node->next = NULL;
            this->lastChild = node;
        }
    }
}

MarkdownNode* MarkdownNode::getFirstChild() const
{
    return this->firstChild;
}

MarkdownNode* MarkdownNode::getLastChild() const
{
    return this->lastChild;
}

MarkdownNode* MarkdownNode::getPrevious() const
{
    return this->prev;
}

MarkdownNode* MarkdownNode::getNext() const
{
    return this->next;
}

QString MarkdownNode::toString() const
{
    int left = 20;
    int right = 20;
    QString text = getText();

    if (left > text.length())
    {
        left = text.length();
    }

    right = text.length() - left;

    if (right < 0)
    {
        right = 0;
    }

    return QString("> [lines %1 - %2][col %3, len %5] %6 -> %7")
        .arg(getStartLine())
        .arg(getEndLine())
        .arg(getPosition())
        .arg(getLength())
        .arg(toString(type))
        .arg(getText().left(left) + "..." + getText().right(right));
}

bool MarkdownNode::isInvalid() const
{
    return (Invalid == this->type);
}

MarkdownNode::NodeType MarkdownNode::getType() const
{
    return this->type;
}

int MarkdownNode::getPosition() const
{
    return position;
}

int MarkdownNode::getLength() const
{
    return length;
}

int MarkdownNode::getStartLine() const
{
    return startLine;
}

int MarkdownNode::getEndLine() const
{
    return endLine;
}

QString MarkdownNode::getText() const
{
    return text;
}

bool MarkdownNode::isBlockType() const
{
    return
    (
        (this->type >= FirstBlockType)
        && (this->type <= LastBlockType)
    );
}

bool MarkdownNode::isInlineType() const
{
    return
    (
        (this->type >= FirstInlineType)
        && (this->type <= LastInlineType)
    );
}

int MarkdownNode::getHeadingLevel() const
{
    return this->headingLevel;
}

bool MarkdownNode::isSetextHeading() const
{
    return
        (
            (Heading == this->type)
            &&
            ((getEndLine() - getStartLine() + 1) > 1)
        );
}

bool MarkdownNode::isAtxHeading() const
{
    return
        (
            (Heading == this->type)
            &&
            !isSetextHeading()
        );
}

bool MarkdownNode::isInsideBlockquote() const
{
    MarkdownNode* parent = this->getParent();

    while (NULL != parent)
    {
        if (BlockQuote == parent->getType())
        {
            return true;
        }

        parent = parent->getParent();
    }

    return false;
}

bool MarkdownNode::isFencedCodeBlock() const
{
    return fenceChar != '\0';
}

bool MarkdownNode::isNumberedListItem() const
{
    return
    (
        (ListItem == type)
        &&
        (this->getParent() != NULL)
        &&
        (NumberedList == this->getParent()->getType())
    );
}

int MarkdownNode::getListItemNumber() const
{
    int startNum = this->listStartNum;
    int count = 1;

    MarkdownNode* p = this->prev;

    while ((p != NULL) && (p != this->parent))
    {
        count++;
        p = p->getPrevious();
    }

    return startNum + count;
}

bool MarkdownNode::isBulletListItem() const
{
    return
    (
        (ListItem == type)
        &&
        (this->getParent() != NULL)
        &&
        (BulletList == this->getParent()->getType())
    );
}

MarkdownNode::NodeType MarkdownNode::getNodeType(cmark_node* node)
{
    switch (cmark_node_get_type(node))
    {
        case CMARK_NODE_DOCUMENT:
            return Document;
        case CMARK_NODE_BLOCK_QUOTE:
            return BlockQuote;
        case CMARK_NODE_LIST:
            switch (cmark_node_get_list_type(node))
            {
                case CMARK_ORDERED_LIST:
                    return NumberedList;
                case CMARK_BULLET_LIST:
                    return BulletList;
                default:
                    return Invalid;
            }
            break;
        case CMARK_NODE_ITEM:
            if (0 == strcmp(cmark_node_get_type_string(node), "tasklist"))
            {
                return TaskListItem;
            }

            return ListItem;
        case CMARK_NODE_CODE_BLOCK:
            return CodeBlock;
        case CMARK_NODE_HTML_BLOCK:
            return HtmlBlock;
        case CMARK_NODE_PARAGRAPH:
            return Paragraph;
        case CMARK_NODE_HEADING:
            return Heading;
        case CMARK_NODE_THEMATIC_BREAK:
            return ThematicBreak;
        case CMARK_NODE_FOOTNOTE_DEFINITION:
            return FootnoteDefinition;
        case CMARK_NODE_TEXT:
            return Text;
        case CMARK_NODE_SOFTBREAK:
            return Softbreak;
        case CMARK_NODE_LINEBREAK:
            return Linebreak;
        case CMARK_NODE_CODE:
            return Code;
        case CMARK_NODE_HTML_INLINE:
            return HtmlInline;
        case CMARK_NODE_EMPH:
            return Emph;
        case CMARK_NODE_STRONG:
            return Strong;
        case CMARK_NODE_LINK:
            return Link;
        case CMARK_NODE_IMAGE:
            return Image;
        case CMARK_NODE_FOOTNOTE_REFERENCE:
            return FootnoteReference;
        default:
            if (0 == strcmp(cmark_node_get_type_string(node), "table"))
            {
                return Table;
            }
            else if (0 == strcmp(cmark_node_get_type_string(node), "table_row"))
            {
                return TableRow;
            }
            else if (0 == strcmp(cmark_node_get_type_string(node), "table_header"))
            {
                return TableHeading;
            }
            else if (0 == strcmp(cmark_node_get_type_string(node), "table_cell"))
            {
                return TableCell;
            }
            else if (0 == strcmp(cmark_node_get_type_string(node), "strikethrough"))
            {
                return Strikethrough;
            }
    }

    return Invalid;
}

QString MarkdownNode::toString(NodeType nodeType) const
{
    switch (nodeType)
    {
        case MarkdownNode::Invalid:
            return "Invalid";
        case MarkdownNode::Document:
            return "Document";
        case MarkdownNode::BlockQuote:
            return "BlockQuote";
        case MarkdownNode::NumberedList:
            return "NumberedList";
        case MarkdownNode::BulletList:
            return "BulletList";
        case MarkdownNode::TaskListItem:
            return "TaskList";
        case MarkdownNode::ListItem:
            return "ListItem";
        case MarkdownNode::CodeBlock:
            return "CodeBlock";
        case MarkdownNode::HtmlBlock:
            return "HtmlBlock";
        case MarkdownNode::Paragraph:
            return "Paragraph";
        case MarkdownNode::Heading:
            return "Heading";
        case MarkdownNode::ThematicBreak:
            return "ThematicBreak";
        case MarkdownNode::FootnoteDefinition:
            return "FootnoteDefinition";
        case MarkdownNode::Table:
            return "Table";
        case MarkdownNode::TableHeading:
            return "TableHeading";
        case MarkdownNode::TableRow:
            return "TableRow";
        case MarkdownNode::TableCell:
            return "TableCell";
        case MarkdownNode::Text:
            return "Text";
        case MarkdownNode::Softbreak:
            return "Softbreak";
        case MarkdownNode::Linebreak:
            return "Linebreak";
        case MarkdownNode::Code:
            return "Code";
        case MarkdownNode::HtmlInline:
            return "HtmlInline";
        case MarkdownNode::Emph:
            return "Emph";
        case MarkdownNode::Strong:
            return "Strong";
        case MarkdownNode::Link:
            return "Link";
        case MarkdownNode::Image:
            return "Image";
        case MarkdownNode::Strikethrough:
            return "Strikethrough";
        case MarkdownNode::FootnoteReference:
            return "FootnoteReference";
        default:
            return QString(static_cast<std::uint32_t>(nodeType));
    }
}