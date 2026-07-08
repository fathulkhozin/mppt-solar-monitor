import reflex as rx

config = rx.Config(
    app_name="mppt_monitor",
    db_url="sqlite:///reflex.db",
    plugins=[
        rx.plugins.SitemapPlugin(),
        rx.plugins.TailwindV4Plugin(),
    ]
)