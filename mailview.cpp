#include <gtkmm.h>
#include <mimetic/mimetic.h>
#include <iostream>
#include <iterator>
#include <vector>

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

Gtk::Widget *build_mime_widget(const mimetic::MimeEntity &entity) {
  mimetic::ContentType content_type = entity.header().contentType();
  if (content_type.isMultipart()) {
    if (content_type.subtype() == "mixed") {
      return
	new MultipartMixedWidget(static_cast<const mimetic::MultipartMixed &>(entity));
    } else {
      return new UnknownMimeWidget(entity);
    }
  } else {
    return new UnknownMimeWidget(entity);
  }
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
