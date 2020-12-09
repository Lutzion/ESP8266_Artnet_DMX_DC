jQuery( document ).ready(function( $ ) {
  $(document).ready( updateDiv );
});

// update the content of div elements
function updateDiv()
{
$.getJSON( "json", function() {
  //alert( "success" );
})
  .done(function(data) {
    console.log(data);
    $.each(data, function(key, value) {
      elem = document.getElementById(key);
      if (elem)
        elem.innerHTML = value;
    });
    //alert( "done" );
  })
  .fail(function() {
    //alert( "error" );
  })
  .always(function() {
    //alert( "finished" );
  });
}

function startWS()
{
  Socket = new WebSocket('ws://' + window.location.hostname + ':443/');
  var ids = ['uptime', 'packets', 'seqErr', 'fps'];
  
  Socket.onmessage = function(evt)
  {
    var strElements = evt.data.split('#'); 

    for (i = 0; i < strElements.length; ++i)
    {
      if (i < ids.length)
      {
        elem = document.getElementById(ids[i]) ;
        
        if (elem)
        {
          //elem[0].value = strElements[i];
          elem.innerHTML = strElements[i];
        }
      }
    }
  }
}

