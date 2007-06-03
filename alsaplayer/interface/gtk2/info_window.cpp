
#include "info_window.h"


static GtkWidget*
create_info_window()
{
	GtkWidget *frame;
	GtkWidget *main_box;
	GtkWidget *speed_label;
	GtkWidget *balance_label;
	GtkWidget *title_label;
	GtkWidget *format_label;
	GtkWidget *volume_label;
	GtkWidget *position_label;
	
	frame = gtk_frame_new(NULL);
	gtk_widget_set_size_request(frame, 500, 50);
	main_box = gtk_layout_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(frame), main_box);
	
	speed_label = gtk_label_new("Speed: 100%");
	g_object_set_data(G_OBJECT(frame), "speed_label", speed_label);
	gtk_layout_put(GTK_LAYOUT(main_box), speed_label, 0, 0);	
	balance_label = gtk_label_new("Balance: center");
	g_object_set_data(G_OBJECT(frame), "balance_label", balance_label);
	gtk_layout_put(GTK_LAYOUT(main_box), balance_label, 0, 25);
	
	title_label = gtk_label_new("TITLE");
	g_object_set_data(G_OBJECT(frame), "title_label", title_label);
	gtk_layout_put(GTK_LAYOUT(main_box), title_label, 100, 0);
	format_label = gtk_label_new("format");
	g_object_set_data(G_OBJECT(frame), "format_label", format_label);
	gtk_layout_put(GTK_LAYOUT(main_box), format_label, 100, 25);
	
	volume_label = gtk_label_new("VOLUME: 100%");
	g_object_set_data(G_OBJECT(frame), "volume_label", volume_label);
	gtk_layout_put(GTK_LAYOUT(main_box), volume_label, 400, 0);
	position_label = gtk_label_new("00:00/03:00");
	g_object_set_data(G_OBJECT(frame), "position_label", position_label);
	gtk_layout_put(GTK_LAYOUT(main_box), position_label, 400, 25);
	
	return frame;	
}

InfoWindow::InfoWindow()
{
	window = create_info_window();
}

InfoWindow::~InfoWindow()
{
	gtk_widget_destroy(window);
}

void InfoWindow::set_volume(const gchar *text)
{
	volume = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "volume_label"));
	gtk_label_set_text (GTK_LABEL(volume), text);
}

void InfoWindow::set_balance(const gchar *text)
{
	balance = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "balance_label"));
	gtk_label_set_text (GTK_LABEL(balance), text);
}

void InfoWindow::set_position(const gchar *text)
{
	position = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "position_label"));
	gtk_label_set_text (GTK_LABEL(position), text);
}

void InfoWindow::set_title(const gchar *text)
{
	title = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "title_label"));
	gtk_label_set_text (GTK_LABEL(title), text);
}

void InfoWindow::set_format(const gchar *text)
{
	format = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "format_label"));
	gtk_label_set_text (GTK_LABEL(format), text);
}

void InfoWindow::set_speed(const gchar *text)
{
	speed = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "speed_label"));
	gtk_label_set_text (GTK_LABEL(speed), text);
}
