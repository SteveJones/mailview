/*
  Copyright 2011-2013 Steve Jones <steve@secretvolcanobase.org>
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <gtkmm.h>
#include <mimetic/mimetic.h>
#include <iostream>
#include <iterator>
#include <vector>
extern "C" {
#include <webkit/webkit.h>
#include <gtk/gtkscrolledwindow.h>
}

Gtk::Widget *build_mime_widget(const mimetic::MimeEntity &);

class UnknownMimeWidget : public Gtk::TextView {
public:
  UnknownMimeWidget(const mimetic::MimeEntity &);

private:
  Glib::RefPtr<Gtk::TextBuffer> m_text_buffer;
};

UnknownMimeWidget::UnknownMimeWidget(const mimetic::MimeEntity &entity) 
  : Gtk::TextView()
{
  std::stringstream ss;
  ss << entity;
  m_text_buffer = Gtk::TextBuffer::create();
  m_text_buffer->set_text(ss.str());
  set_buffer(m_text_buffer);
}

class AttachmentWidget : public Gtk::VBox {
public:
  AttachmentWidget(const mimetic::MimeEntity *entity, Gtk::Widget *widget = NULL);

private:
  void show_inline();
  void hide_inline();

  const mimetic::MimeEntity *m_entity;
  Gtk::Widget *m_widget;
  Gtk::HBox m_header;
  std::string m_filename;
  Gtk::Label m_filename_label;
  Gtk::Button *m_show_button;
  sigc::connection m_button_handler;
};

AttachmentWidget::AttachmentWidget(const mimetic::MimeEntity *entity, Gtk::Widget *widget)
  : Gtk::VBox(),
    m_entity(entity),
    m_widget(widget)
{
  mimetic::ContentDisposition content_disposition
    = m_entity->header().contentDisposition();
  m_filename = content_disposition.param("filename");
  if (m_filename == "") {
    m_filename = "unknown";
  }
  m_filename_label.set_text(m_filename);
  m_header.pack_start(m_filename_label, Gtk::PACK_SHRINK, 0);

  if (m_widget) {
    m_show_button = new Gtk::Button("Show");
    m_button_handler = m_show_button->signal_clicked()
      .connect(sigc::mem_fun(*this, &AttachmentWidget::show_inline));
    m_header.pack_end(*m_show_button, Gtk::PACK_SHRINK, 0);
  }
  pack_start(m_header, Gtk::PACK_SHRINK, 0);
}

void AttachmentWidget::show_inline()
{
  pack_start(*m_widget);
  m_show_button->property_label() = "Hide";
  m_button_handler.disconnect();
  m_button_handler = m_show_button->signal_clicked()
    .connect(sigc::mem_fun(*this, &AttachmentWidget::hide_inline));
  show_all();
}

void AttachmentWidget::hide_inline()
{
  remove(*m_widget);
  m_show_button->property_label() = "Show";
  m_button_handler.disconnect();
  m_button_handler = m_show_button->signal_clicked()
    .connect(sigc::mem_fun(*this, &AttachmentWidget::show_inline));
}

class MultipartMixedWidget : public Gtk::ScrolledWindow {
public:
  MultipartMixedWidget(const mimetic::MultipartMixed &);

private:
  Gtk::VBox m_vbox;
  std::vector<Gtk::Widget *> m_inline_parts;
};

MultipartMixedWidget::MultipartMixedWidget(const mimetic::MultipartMixed &entity)
  : Gtk::ScrolledWindow()
{
  set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  add(m_vbox);
  for (mimetic::MimeEntityList::const_iterator i = entity.body().parts().begin();
       i != entity.body().parts().end();
       ++i) {
    Gtk::Widget *widget = build_mime_widget(**i);
    if ((*i)->header().contentDisposition().type() == "attachment") {
      AttachmentWidget *attachment_widget = new AttachmentWidget(*i, widget);
      m_vbox.pack_start(*attachment_widget);
    } else {
      m_inline_parts.push_back(widget);
      m_vbox.pack_start(*m_inline_parts.back(), Gtk::PACK_EXPAND_WIDGET, 1);
    }
  }
}

class MultipartAlternativeWidget : public Gtk::Notebook {
public:
  MultipartAlternativeWidget(const mimetic::MultipartAlternative &);

private:
  std::vector<Gtk::Widget *> m_alternates;
};

MultipartAlternativeWidget::MultipartAlternativeWidget(const mimetic::MultipartAlternative &entity)
  : Gtk::Notebook()
{
  for (mimetic::MimeEntityList::const_iterator i = entity.body().parts().begin();
       i != entity.body().parts().end();
       ++i) {
    m_alternates.push_back(build_mime_widget(**i));
    Gtk::ScrolledWindow *scrolled_window = new Gtk::ScrolledWindow();
    scrolled_window->add(*m_alternates.back());
    scrolled_window->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    append_page(*scrolled_window, (*i)->header().contentType().str());
  }
}

template<class OutputIterator>
void decode_body(const mimetic::TextEntity &entity, OutputIterator out) {
  if (entity.hasField("Content-Transfer-Encoding")) {
    std::string content_transfer_encoding
      = entity.header().contentTransferEncoding().mechanism();
    if (content_transfer_encoding == "quoted-printable") {
      mimetic::QP::Decoder qp;
      decode(entity.body().begin(), entity.body().end(), qp, out);
      return;
    }
    if (content_transfer_encoding == "base64") {
      mimetic::Base64::Decoder b64;
      decode(entity.body().begin(), entity.body().end(), b64, out);
      return;
    }
  }
  copy(entity.body().begin(), entity.body().end(), out);
}

class TextPlainWidget : public Gtk::TextView {
public:
  TextPlainWidget(const mimetic::TextEntity &);

private:
  Glib::RefPtr<Gtk::TextBuffer> m_text_buffer;
};

TextPlainWidget::TextPlainWidget(const mimetic::TextEntity &entity) 
  : Gtk::TextView()
{
  std::string body;
  m_text_buffer = Gtk::TextBuffer::create();
  decode_body(entity, back_inserter(body));
  std::string charset = entity.header().contentType().param("charset");
  if (charset == "") {
    charset == "us-ascii";
  }
  if (charset != "utf-8") {
    Glib::IConv iconv("utf-8", charset);
    body = iconv.convert(body);
  }
  m_text_buffer->set_text(body);
  set_wrap_mode(Gtk::WRAP_WORD_CHAR);
  set_buffer(m_text_buffer);
}

class TextHTMLWidget : public Gtk::Widget {
public:
  TextHTMLWidget(const mimetic::TextEntity &);

private:
  WebKitWebView *m_web_view;
};

TextHTMLWidget::TextHTMLWidget(const mimetic::TextEntity &entity)
  : Gtk::Widget(webkit_web_view_new())
{
  m_web_view = WEBKIT_WEB_VIEW(gobj());
  std::string body;
  decode_body(entity, back_inserter(body));
  mimetic::ContentType content_type = entity.header().contentType();
  std::string mimetype = "text/html";
  std::string charset = content_type.param("charset");
  if (charset == "") {
    charset = "UTF-8";
  }
  webkit_web_view_load_string(m_web_view, body.c_str(),
  			      mimetype.c_str(), charset.c_str(), NULL);
  //webkit_web_view_load_uri(WEBKIT_WEB_VIEW(m_web_view), "http://www.google.co.uk");
  show_all();
}

Gtk::Widget *build_mime_widget(const mimetic::MimeEntity &entity) {
  mimetic::ContentType content_type = entity.header().contentType();
  if (content_type.isMultipart()) {
    if (content_type.subtype() == "mixed") {
      return
	new MultipartMixedWidget(static_cast<const mimetic::MultipartMixed &>(entity));
    } else if (content_type.subtype() == "alternative") {
      return
	new MultipartAlternativeWidget(static_cast<const mimetic::MultipartAlternative &>(entity));
    }
  } else if (content_type.type() == "text") {
    if (content_type.subtype() == "plain") {
      return
	new TextPlainWidget(static_cast<const mimetic::TextPlain &>(entity));
    } else if (content_type.subtype() == "html") {
      return
	new TextHTMLWidget(static_cast<const mimetic::TextEntity &>(entity));
    }
  }
  // fallback
  return new UnknownMimeWidget(entity);
}

int main(int argc, char *argv[]) {
  Gtk::Main kit(argc, argv);
  std::istreambuf_iterator<char> begin(std::cin), end;
  mimetic::MimeEntity message(begin, end);
  Gtk::Window main_window;
  Gtk::Widget *message_widget = build_mime_widget(message);
  main_window.property_default_height() = 600;
  main_window.property_default_width() = 800;
  main_window.add(*message_widget);
  main_window.show_all_children();
  Gtk::Main::run(main_window);
}
