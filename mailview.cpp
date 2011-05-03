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

class MultipartMixedWidget : public Gtk::VBox {
public:
  MultipartMixedWidget(const mimetic::MultipartMixed &);

private:
  std::vector<Gtk::Widget *> m_parts;
};

MultipartMixedWidget::MultipartMixedWidget(const mimetic::MultipartMixed &entity)
  : Gtk::VBox()
{
  for (mimetic::MimeEntityList::const_iterator i = entity.body().parts().begin();
       i != entity.body().parts().end();
       ++i) {
    m_parts.push_back(build_mime_widget(**i));
    pack_start(*m_parts.back(), Gtk::PACK_SHRINK, 0);
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
    append_page(*m_alternates.back(), (*i)->header().contentType().str());
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
  set_wrap_mode(Gtk::WRAP_WORD_CHAR);
  m_text_buffer->set_text(body);
  set_buffer(m_text_buffer);
}

class TextHTMLWidget : public Gtk::ScrolledWindow {
public:
  TextHTMLWidget(const mimetic::TextEntity &);

private:
  GtkWidget *m_web_view;
};

TextHTMLWidget::TextHTMLWidget(const mimetic::TextEntity &entity)
  : Gtk::ScrolledWindow()
{
  m_web_view = webkit_web_view_new();
  gtk_container_add(GTK_CONTAINER(gobj()), m_web_view);
  std::string body;
  decode_body(entity, back_inserter(body));
  mimetic::ContentType content_type = entity.header().contentType();
  std::string mimetype = "text/html";
  std::string charset = content_type.param("charset");
  if (charset == "") {
    charset = "UTF-8";
  }
  std::cout << mimetype << std::endl;
  webkit_web_view_load_string(WEBKIT_WEB_VIEW(m_web_view), body.c_str(),
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
  main_window.add(*message_widget);
  main_window.show_all_children();
  Gtk::Main::run(main_window);
}
