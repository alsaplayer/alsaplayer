#ifndef INFO_WINDOW_H_
#define INFO_WINDOW_H_

#include <cstdio>
#include <gtk/gtk.h>

class InfoWindow
{
	private:
		GtkWidget *window;
		GtkWidget *volume;
		GtkWidget *balance;
		GtkWidget *title;
		GtkWidget *format;
		GtkWidget *speed;
		GtkWidget *position;
	public:
		InfoWindow();
		~InfoWindow();
		
		GtkWidget *GetWindow() {return window;}
		void set_volume(const gchar*);
		void set_balance(const gchar*);
		void set_title(const gchar*);
		void set_format(const gchar*);
		void set_speed(const gchar*);
		void set_position(const gchar*);
};
#endif /*INFO_WINDOW_H_*/
