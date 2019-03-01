#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>

#include "container_linux.h"

struct pango_font
{
	PangoFontDescription *font;
	bool underline;
	bool strikethrough;
};

class lh_widget : public container_linux
{
	public:
		lh_widget();
		~lh_widget();

		GtkWidget *get_widget() const;

		/* Methods that litehtml calls */
		void set_caption(const litehtml::tchar_t* caption);
		void set_base_url(const litehtml::tchar_t* base_url);
		void on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el);
		void set_cursor(const litehtml::tchar_t* cursor);
		void import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl);
		void get_client_rect(litehtml::position& client) const;
		inline const litehtml::tchar_t *get_default_font_name() const { return m_font_name; };
		GdkPixbuf *get_image(const litehtml::tchar_t* url, bool redraw_on_ready);

		inline int get_default_font_size() const { return m_font_size; };
		litehtml::uint_ptr create_font(const litehtml::tchar_t* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm);
		void delete_font(litehtml::uint_ptr hFont);
		int text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont);
		void draw_text(litehtml::uint_ptr hdc, const litehtml::tchar_t* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos);

		void draw(cairo_t *cr);
		void redraw();
		void open_html(const gchar *contents);
		void clear();
		void update_cursor();
		void update_font();
		void print();

		const litehtml::tchar_t *get_href_at(const gint x, const gint y) const;
		void popup_context_menu(const litehtml::tchar_t *url, GdkEventButton *event);
		const litehtml::tstring fullurl(const litehtml::tchar_t *url) const;

		litehtml::document::ptr m_html;
		litehtml::tstring m_clicked_url;
		litehtml::tstring m_base_url;

	private:
		void paint_white();

		gint m_rendered_width;
		GtkWidget *m_drawing_area;
		GtkWidget *m_scrolled_window;
		GtkWidget *m_viewport;
		GtkWidget *m_context_menu;
		litehtml::context m_context;
		gint m_height;
		litehtml::tstring m_cursor;

		litehtml::tchar_t *m_font_name;
		int m_font_size;
};