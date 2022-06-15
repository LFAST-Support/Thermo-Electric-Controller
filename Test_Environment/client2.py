import sys

from PySide6.QtCore import *
from PySide6.QtWidgets import *
from PySide6.QtGui import *

#from client import change_module_button_handler

option_module_id = 0

def center_widget( widget ):
    h_box = QWidget()
    h_box_layout = QHBoxLayout()
    h_box.setLayout( h_box_layout )
    h_box_layout.addWidget( QLabel( '' ), 10 )
    h_box_layout.addWidget( widget, 0 )
    h_box_layout.addWidget( QLabel( '' ), 10 )
    return h_box


class MainWindow(QMainWindow):

    def __init__(self):
        super().__init__()
        self.setWindowTitle( f'TEC Client v{2} - Module {option_module_id}' )






        window = QWidget()
        v_box_layout = QVBoxLayout()
        v_box_layout.setSpacing( 0 )
        window.setLayout( v_box_layout )
        self.setCentralWidget( window )

        # The Change Module button
        change_module_controls = QWidget()
        h_box_layout = QHBoxLayout()
        change_module_controls.setLayout( h_box_layout )
        change_module_input = QLineEdit( f'{option_module_id}' )
        only_int = QIntValidator()
        change_module_input.setValidator( only_int )
        h_box_layout.addWidget( change_module_input )
        change_module_button = QPushButton( 'Change Module' )
        #change_module_button.clicked.connect( change_module_button_handler )
        h_box_layout.addWidget( change_module_button )
        v_box_layout.addWidget( center_widget( change_module_controls ) )

        change_module_button = QPushButton("Change Module")
        change_module_button.setCheckable(True)
        #change_module_button.clicked.connect(change_module_button_handler)

        self.setCentralWidget(change_module_button)

    




























app = QApplication(sys.argv)

window = MainWindow()
window.show()

app.exec()