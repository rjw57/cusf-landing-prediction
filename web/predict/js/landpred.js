var g_map_object = null;
function init_map() {
        var latlng = new google.maps.LatLng(52, 0);
        var options = {
                zoom: 8,
                center: latlng,
                mapTypeId: google.maps.MapTypeId.ROADMAP
        };
        var map = new google.maps.Map(document.getElementById('map_canvas'), options)
        g_map_object = map;
}

function add_new_prediction(prediction) {
        $('#prediction-running').fadeOut();
        var path = []
        var max_height = -10;
        var max_point = null;
        $.each(prediction.output, function(idx, entry) {
                if(entry.length >= 4) {
                        var point = new google.maps.LatLng( 
                                parseFloat(entry[1]), parseFloat(entry[2])
                        );
                        if(parseFloat(entry[3]) > max_height) {
                                max_height = parseFloat(entry[3]);
                                max_point = point;
                        }
                        path.push(point);
                }
        });

        var path_polyline = new google.maps.Polyline({
                path: path,
                strokeColor: '#000000',
                strokeWeight: 3,
                strokeOpacity: 0.75,
        });
        path_polyline.setMap(g_map_object);

        var pop_icon = new google.maps.MarkerImage('images/pop-marker.png',
                new google.maps.Size(16, 16),
                new google.maps.Point(0, 0),
                new google.maps.Point(8, 8));

        var pop_marker = new google.maps.Marker({
                position: max_point,
                map: g_map_object,
                icon: pop_icon,
                title: 'Baloon burst (max. altitude: ' + max_height + 'm)',
        });
}

$(document).ready(function() {
        init_map();

        $('form#scenario').ajaxForm({
                'success': add_new_prediction,
                'dataType': 'json',
        });
        $('form#scenario').submit(function() {
                return false; // stop normal browser submit
        });

        var new_pred_dialog = $('#new_prediction').dialog({
                'width': $('#scenario_table').width(),
                'height': $('#scenario_table').height() + 120,
                'bgiframe': true,
                'buttons': {
                        'Run prediction': function() { 
                                $('#prediction-running').fadeIn();
                                $('form#scenario').submit();
                         }
                },
        });
        $('#new-prediction-button').click( function() { new_pred_dialog.dialog('open'); } );

        /*
        $('#error-dialog').dialog({
                'bgiframe': true,
                'modal': true,
                'buttons' : { Ok: function() { $(this).dialog('close'); } },
        });
        */
});

// vim:et:ts=8:sw=8:autoindent
