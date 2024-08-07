
#include "driver/uart.h"
#include "soc/uart_struct.h"

uint8_t state[ 22 ];
int connected = 0;

void GO_init() {
   state[ 0 ] = 0; // no new data
	const uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config( UART_GO, &uart_config);
	uart_set_pin( UART_GO, UART_GO_TXD_PIN, UART_GO_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	// We won't use a buffer for sending data.
	uart_driver_install( UART_GO, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

int* getChecksum( uint8_t* data, int length ){
   int *result = malloc( 2 );
   result[ 0 ] = 0;
   result[ 1 ] = 0;
   //ESP_LOGI( "TEST", "%d", length );
   for( int i=0; i < length; i++ ){
      result[ 0 ] = result[ 0 ] + data[ i ];
      result[ 1 ] = result[ 1 ] + result[ 0 ];
      //ESP_LOGI( "TEST", "index %d %x %x", i, result[0], result[1] );
      //ESP_LOGI( "1", "%x", result[1] );
   }
   result[ 0 ] = result[ 0 ] % 256;
   result[ 1 ] = result[ 1 ] % 256;
   //ESP_LOGI( "TEST", "result %x %x", result[0], result[1] );
   return result;
}

int GO_sendCustomData( uint8_t device, uint8_t event, uint16_t value ) {
   gpio_set_level( FMS_LED, 0 );
	uint8_t msg[ 33 ];
   for( int i = 0; i < 33; i++) {
      msg[ i ] = 0x00;
   }
   msg[  0 ] = 0x02; // start frame
   msg[  1 ] = 0x82; // Msg Type 0x82: Free Format Third-Party Data
   msg[  2 ] = 0x1b; // 27 bytes
   msg[  3 ] = 0x46; // GOWIT start

   msg[  4 ] = device; // device_type
   msg[  5 ] = event; // event_type
   msg[  6 ] = ( value & 0x00ff ); // value
   msg[  7 ] = ( value & 0xff00 ) >> 8; // value

   msg[ 29 ] = 0x80; // GOWIT end
   msg[ 32 ] = 0x03; // end frame

   uint8_t data_request[] = { 0x02, 0x85, 0x00, 0xff, 0xff, 0x03 };
   int* checksum = getChecksum( data_request, 3 );
   data_request[ 3 ] = checksum[ 0 ];
   data_request[ 4 ] = checksum[ 1 ];
	uart_write_bytes( UART_GO, data_request, sizeof( data_request ) );

   // waiting for data
   while( state[ 0 ] == 0 );
   state[ 0 ] = 0;

   for( int i = 1; i < sizeof( state ); i++ ) {
      msg[ 7 + i ] = state[ i ];
   }

   checksum = getChecksum( msg, 30 );
   ESP_LOGI( "CHECKSUM", "%x %x", checksum[0], checksum[1] );
   msg[ 30 ] = checksum[ 0 ];
   msg[ 31 ] = checksum[ 1 ];
   for( int i=0; i < 33; i++ )
      ESP_LOGI( "MSG", "%x", msg[i] );

   gpio_set_level( FMS_LED, 1 );

   return uart_write_bytes( UART_GO, msg, sizeof( msg ) );
}

void GO_handShake() {
	ESP_LOGI( "GO_TX", "Init Handshake" );
	char key[] = { 0x55 };
	while( connected == 0 ) {
      gpio_set_level( FMS_LED, 0 );
		uart_write_bytes( UART_GO, key, sizeof( key ) );
		vTaskDelay( 500 / portTICK_PERIOD_MS );
      gpio_set_level( FMS_LED, 1 );
      vTaskDelay( 500 / portTICK_PERIOD_MS );
	}
	ESP_LOGI( "GO_TX", "Handshake confirmation message" );
   uint8_t data[] = { 0x02, 0x81, 0x04, 0x0C, 0x10, 0x00, 0x00 };
   int size = sizeof( data );
   int* checksum = getChecksum( data, size );
   ESP_LOGI( "CHECKSUM", "%x %x", checksum[0], checksum[1] );
   char* msg = malloc( size + 3 ); // n*Data + 2*Checksum + 1*End
   for( int i=0; i < size; i++ )
      msg[ i ] = data[ i ];
   msg[ size + 0 ] = checksum[ 0 ];
   msg[ size + 1 ] = checksum[ 1 ];
   free( checksum );
   msg[ size + 2 ] = 0x03;

   // for( int i=0; i < size+3; i++ )
   //    ESP_LOGI( "MSG", "%x", message[i] );
	//char msg[] = { 0x02, 0x81, 0x04, 0x0C, 0x10, 0x00, 0x00, 0xA3, 0x88, 0x03 };

	uart_write_bytes( UART_GO, msg, size+3 );
   free( msg );
}

static void GO_rxTask() {
   ESP_LOGI( "GO", "Init RX Task" );

	uint8_t* data = (uint8_t*) malloc( RX_BUF_SIZE+1 );

	while( 1 ) {
		const int rxBytes = uart_read_bytes( UART_GO, data, RX_BUF_SIZE, 100 / portTICK_RATE_MS);
		// ESP_LOGI("APP", "%s", (const char *)data );
		// ESP_LOGI( "GO_RX", "%d", rxBytes );
		if( rxBytes == 6 ){
         // valid GO frame 0x01
         if ( data[ 0 ] == 0x02 && data[ 1 ] == 0x01 && data[ 5 ] == 0x03 ) {
            ESP_LOGI( "GO_RX", "Handshake request message");
            connected = 1;
         }
         // valid GO frame 0x02
         if ( data[ 0 ] == 0x02 && data[ 1 ] == 0x02 && data[ 5 ] == 0x03 ) {
            ESP_LOGI( "GO_RX", "Data Acknowledge Message");
         }
      }
      if (rxBytes >= 40 ) {
         // valid Data frame 0x21
         if ( data[ 0 ] == 0x02 && data[ 1 ] == 0x21 && data[ data[2] + 5 ] == 0x03 ) {
            ESP_LOGI("GO_RX", "L:%d DATA: %x%x %d ", rxBytes, data[0], data[1], data[2] );
            uint32_t timestamp = data[3]*0x1 + data[4]*0x100 + data[5]*0x10000 + data[6]*0x1000000 + 1009843200;
            ESP_LOGI("GO_RX", "TS:%d %X%X%X%X", timestamp, data[6],data[5],data[4],data[3]  );
            // NEW data
            state[  0 ] = 1;
            // 4*timestamp
            state[  1 ] = data[  3 ];
            state[  2 ] = data[  4 ];
            state[  3 ] = data[  5 ];
            state[  4 ] = data[  6 ];
            // 4*latitude
            state[  5 ] = data[  7 ];
            state[  6 ] = data[  8 ];
            state[  7 ] = data[  9 ];
            state[  8 ] = data[ 10 ];
            // 4*longitude
            state[  9 ] = data[ 11 ];
            state[ 10 ] = data[ 12 ];
            state[ 11 ] = data[ 13 ];
            state[ 12 ] = data[ 14 ];
            // 1*speed
            state[ 13 ] = data[ 15 ];
            // 4*driver_id
            state[ 14 ] = data[ 39 ];
            state[ 15 ] = data[ 40 ];
            state[ 16 ] = data[ 41 ];
            state[ 17 ] = data[ 42 ];
            // 4*trip_duration
            state[ 18 ] = data[ 31 ];
            state[ 19 ] = data[ 32 ];
            state[ 20 ] = data[ 33 ];
            state[ 21 ] = data[ 34 ];
         }
      }
	}
	free(data);
}
